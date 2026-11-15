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
# 用安装树里的 runner：同时验证安装树自包含、可执行
RUNNER="${STAGE}/bin/native_runner"

# libdir 在 Debian/Ubuntu 可能是 lib/x86_64-linux-gnu，不能写死 lib/
PC_FILE="$(find "${STAGE}" -name 'starletto.pc' -print -quit)"
for required in "${INCLUDE_DIR}/basement/runtime/component/component.h" \
                "${INCLUDE_DIR}/basement/schema/builtin/runtime/component_load_plan.s.h" \
                "${SCHEMA_TOOL}"; do
    if [ ! -e "${required}" ]; then
        echo "[E] install surface missing: ${required}" >&2
        exit 1
    fi
done
if [ -z "${PC_FILE}" ]; then
    echo "[E] install surface missing: starletto.pc" >&2
    exit 1
fi
MODULE="${WORK}/oot_module"
mkdir -p "${MODULE}/schema"
cat > "${MODULE}/module.conf" <<'CONF'
module {
    name = oot
    version = 0.1.0
}

components [component.conf]
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
    auto process_once(const star::runtime::FrameInputs&) -> bool override {
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

echo "[i] compiling out-of-tree plugin"
c++ -std=c++20 -O2 -fPIC -shared \
    -I"${INCLUDE_DIR}" -I"${MODULE}" \
    "${MODULE}/register.cc" -o "${MODULE}/liboot.so" -pthread

echo "[i] doctor --deep"
"${RUNNER}" --doctor --deep "${MODULE}/module.conf" >/dev/null

echo "[i] run assembled module"
"${RUNNER}" --frames=0 --sleep-ms=0 "${MODULE}/module.conf" >/dev/null

# 安装树里的静态 runner 必须能直接装配内置模块（不 dlopen 模块库）
STATIC_RUNNER="${STAGE}/bin/native_runner_static"
if [ ! -x "${STATIC_RUNNER}" ]; then
    echo "[E] install surface missing: native_runner_static" >&2
    exit 1
fi
"${STATIC_RUNNER}" --frames=20 --sleep-ms=50 \
    "${STAGE}/modules/setting/config/component.conf" \
    "${STAGE}/modules/setting/parameter/component.conf" \
    "${STAGE}/modules/perception/prediction/component.conf" \
    "${STAGE}/modules/localization/pose/component.conf" \
    "${STAGE}/modules/control/chassis/component.conf" \
    "${STAGE}/modules/planning/motion/component.conf" \
    "${STAGE}/modules/control/pid/component.conf" >/dev/null
echo "[i] installed static runner ok"

echo "[OK] out-of-tree plugin smoke passed"
