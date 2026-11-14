#!/usr/bin/env bash
# 源码树外插件冒烟：安装面（headers + 生成头 + schema 工具 + pkg-config 文件）
# 必须足以让一个仓库外的模块完成「生成 .s.h → 编译插件 → doctor --deep → 运行」。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-${ROOT}/build}"
DEST="$(mktemp -d)"
WORK="$(mktemp -d)"
trap 'rm -rf "${DEST}" "${WORK}"' EXIT

PREFIX="$(meson configure "${BUILD_DIR}" | awk '$1 == "prefix" { print $2; exit }')"
if [ -z "${PREFIX}" ]; then
    echo "[E] cannot determine meson prefix" >&2
    exit 1
fi

echo "[i] installing to DESTDIR=${DEST} prefix=${PREFIX}"
DESTDIR="${DEST}" meson install -C "${BUILD_DIR}" >/dev/null
STAGE="${DEST}${PREFIX}"

INCLUDE_DIR="${STAGE}/include/starletto"
SCHEMA_TOOL="${STAGE}/share/starletto/schema/schema_codegen.py"
# 用安装树里的 runner：同时验证 libstar_runtime 的安装 rpath
RUNNER="${STAGE}/bin/native_runner"

for required in "${INCLUDE_DIR}/basement/runtime/component/component.h" \
                "${INCLUDE_DIR}/basement/plugin/star_plugin_abi.h" \
                "${INCLUDE_DIR}/basement/schema/builtin/runtime/component_load_plan.s.h" \
                "${SCHEMA_TOOL}" \
                "${STAGE}/lib/pkgconfig/starletto.pc"; do
    if [ ! -e "${required}" ]; then
        echo "[E] install surface missing: ${required}" >&2
        exit 1
    fi
done
if ! compgen -G "${STAGE}/lib/libstar_runtime*" >/dev/null; then
    echo "[E] install surface missing: libstar_runtime" >&2
    exit 1
fi

MODULE="${WORK}/oot_module"
mkdir -p "${MODULE}/schema"
cat > "${MODULE}/module.conf" <<'CONF'
module {
    name = oot
    version = 0.1.0
}

components [component.conf, c_component.conf]
CONF
cat > "${MODULE}/component.conf" <<'CONF'
component {
    name = oot_node
    type = oot
    component_library = liboot.so
    component_class = oot::OotNode
}

trigger {
    cycle_ms = 10
}
CONF
cat > "${MODULE}/schema/oot.schema" <<'SCHEMA'
schema oot.message
version 1
namespace oot.generated

root OotMsg {
    1 value : int
}
SCHEMA

# 用安装的 codegen 生成模块自己的类型
python3 "${SCHEMA_TOOL}" gen --schema "${MODULE}/schema/oot.schema" --output "${MODULE}/oot.s.h"

cat > "${MODULE}/register.cc" <<'CPP'
#include "basement/core/build_info.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "oot.s.h"

namespace oot {

class OotNode final : public star::runtime::Component {
  public:
    auto process_once() -> bool override {
        auto msg = oot::generated::OotMsg{};
        msg.value = 1;
        (void)msg.encode_binary();
        return true;
    }
};

inline auto register_native_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<OotNode>("oot::OotNode");
}

}  // namespace oot

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    if (registry != nullptr && registry->schema_registry() != nullptr) {
        oot::generated::OotMsg::register_schema(*registry->schema_registry());
    }
    oot::register_native_components(*registry);
}
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
CPP

cat > "${MODULE}/c_component.conf" <<'CONF'
component {
    name = oot_c_node
    type = oot
    component_library = liboot_c.so
    component_class = oot::CNode
}

trigger {
    cycle_ms = 10
}
CONF

# 纯 C 插件：只用安装出来的 C ABI 头，由 C 编译器编成 .so
cat > "${MODULE}/c_register.c" <<'C'
#include "basement/plugin/star_plugin_abi.h"

typedef struct { int calls; } c_node;
static c_node g_node;

static void* c_create(void) { return &g_node; }
static void c_destroy(void* self) { (void)self; }
static int c_process(void* self) { ((c_node*)self)->calls += 1; return 0; }

static const star_plugin_component_v1 k_components[] = {
    {"oot::CNode", c_create, c_destroy, 0, c_process, 0},
};

static const star_plugin_v1 k_plugin = {
    STAR_PLUGIN_ABI_VERSION,
    "oot_c_plugin",
    k_components,
    1,
};

const star_plugin_v1* star_plugin_entry_v1(void) { return &k_plugin; }
C

echo "[i] compiling out-of-tree plugin"
c++ -std=c++20 -O2 -fPIC -shared \
    -I"${INCLUDE_DIR}" -I"${MODULE}" \
    "${MODULE}/register.cc" -o "${MODULE}/liboot.so" -pthread

echo "[i] compiling out-of-tree C ABI plugin"
cc -std=c11 -O2 -fPIC -shared \
    -I"${INCLUDE_DIR}" \
    "${MODULE}/c_register.c" -o "${MODULE}/liboot_c.so"

# 安装出来的共享库必须能被外部程序 dlopen，并报告正确的插件 ABI 版本
cat > "${WORK}/check_runtime_abi.c" <<'C'
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    void* handle = dlopen(argv[1], RTLD_NOW);
    if (handle == NULL) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 3;
    }
    typedef unsigned int (*abi_fn)(void);
    abi_fn abi = (abi_fn)dlsym(handle, "star_runtime_plugin_abi_version");
    if (abi == NULL) {
        fprintf(stderr, "missing star_runtime_plugin_abi_version\n");
        return 4;
    }
    const unsigned int version = abi();
    dlclose(handle);
    if (version != 1u) {
        fprintf(stderr, "unexpected plugin ABI version: %u\n", version);
        return 5;
    }
    return 0;
}
C
cc -std=c11 -O2 "${WORK}/check_runtime_abi.c" -o "${WORK}/check_runtime_abi" -ldl
RUNTIME_LIB="$(compgen -G "${STAGE}/lib/libstar_runtime.*" | head -n 1)"
"${WORK}/check_runtime_abi" "${RUNTIME_LIB}"
echo "[i] installed libstar_runtime ABI probe ok"

echo "[i] doctor --deep"
"${RUNNER}" --doctor --deep "${MODULE}/module.conf" >/dev/null

echo "[i] run assembled module"
"${RUNNER}" --frames=0 --sleep-ms=0 "${MODULE}/module.conf" >/dev/null

echo "[OK] out-of-tree plugin smoke passed"
