#!/bin/bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

log_error() {
    echo "[E] $*" >&2
}

log_info() {
    echo "[I] $*"
}

# 显示帮助
show_help() {
    echo "star - Native-first 构建/运行工具"
    echo "用法:"
    echo "  star build [参数...]               使用 Meson 编译 native runner 与插件"
    echo "  star clean [参数...]               清理 Meson build 目录"
    echo "  star install [参数...]             安装产物到本地目录（DESTDIR 方式）"
    echo "  star run [参数...] <目标...>       运行 native runner（build 产物或安装产物）"
    echo "  star list [参数...] [目标...]      列出模块与组件"
    echo "  star graph [参数...] [目标...]     输出模块依赖图（dot）"
    echo "  star graph --components [目标...]  输出组件/Topic 拓扑图（dot）"
    echo "  star doctor [参数...] [目标...]    诊断 module/component 清单"
    echo "    - 追加 --deep 可检查插件库是否可 dlopen/dlsym"
    echo "  star selftest [参数...]            运行 transport 自检（例如 routed 不重复投递）"
    echo "  star param [参数...] <命令>         typed parameter service 工具（list/get/set, 支持 --format=json）"
    echo "  star schema <命令>                  schema 工具（fmt/gen/compat/lint）"
    echo "  star record info <文件>             查看录制文件信息（消息数/topics/时间范围）"
    echo "  star replay <文件> [speed]          回放录制文件（支持速率控制）"
    echo "  star monitor [目标...]              实时查看 transport topic 统计（频率/延迟/丢帧/QoS）"
    echo "  star check [参数...] [目标...]      CI Guard: 构建后自动化验证（build + doctor + smoke）"
    echo "  star test [参数...]                 运行单元测试（Meson test）"
    echo "  star help                           显示帮助信息"
    echo ""
    echo "目标可以是：module 名称 / module.conf / component.conf / 目录"
}

native_build() {
    local build_dir="${PROJECT_ROOT}/build"
    local reconfigure=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            --reconfigure)
                reconfigure=true
                shift
                ;;
            -h|--help)
                echo "star native-build - 使用 Meson 编译 native runner 与插件"
                echo "用法:"
                echo "  star native-build [--build-dir=<dir>] [--reconfigure]"
                echo ""
                echo "示例:"
                echo "  star native-build"
                echo "  star native-build --reconfigure"
                return 0
                ;;
            *)
                log_error "native-build 不支持参数: $1"
                return 1
                ;;
        esac
    done

    if ! command -v meson >/dev/null 2>&1; then
        log_error "未找到 meson，请先安装: sudo apt-get install -y meson ninja-build"
        return 1
    fi

    if [ "$reconfigure" = true ]; then
        meson setup "$build_dir" --reconfigure
    else
        if [ -d "$build_dir" ]; then
            meson setup "$build_dir" --reconfigure
        else
            meson setup "$build_dir"
        fi
    fi
    meson compile -C "$build_dir"
}

clean_native() {
    local build_dir="${PROJECT_ROOT}/build"
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            -h|--help)
                echo "star clean - 清理 Meson build 目录"
                echo "用法:"
                echo "  star clean [--build-dir=<dir>]"
                echo ""
                echo "示例:"
                echo "  star clean"
                return 0
                ;;
            *)
                log_error "clean 不支持参数: $1"
                return 1
                ;;
        esac
    done
    rm -rf "$build_dir"
    log_info "cleaned: $build_dir"
}

resolve_native_input() {
    local spec="$1"
    local root="${2:-${PROJECT_ROOT}}"

    if [ -z "$spec" ]; then
        return 1
    fi
    if [ -d "$spec" ] || [ -f "$spec" ]; then
        echo "$spec"
        return 0
    fi

    if [[ "$spec" == */* ]]; then
        local module_path="${root}/modules/${spec}"
        if [ -f "${module_path}/module.conf" ]; then
            echo "${module_path}/module.conf"
            return 0
        fi
        if [ -f "${module_path}/component.conf" ]; then
            echo "${module_path}/component.conf"
            return 0
        fi
        return 1
    fi

    if [ -f "${root}/modules/${spec}/module.conf" ]; then
        echo "${root}/modules/${spec}/module.conf"
        return 0
    fi
    if [ -f "${root}/modules/${spec}/component.conf" ]; then
        echo "${root}/modules/${spec}/component.conf"
        return 0
    fi

    local found=""
    found="$(find "${root}/modules" -type f -name "component.conf" 2>/dev/null | grep "/${spec}/component.conf$" | head -n 1 || true)"
    if [ -n "$found" ]; then
        echo "$found"
        return 0
    fi
    found="$(find "${root}/modules" -type f -name "module.conf" 2>/dev/null | grep "/${spec}/module.conf$" | head -n 1 || true)"
    if [ -n "$found" ]; then
        echo "$found"
        return 0
    fi
    return 1
}

install_native() {
    local build_dir="${PROJECT_ROOT}/build"
    local install_dir="${PROJECT_ROOT}/artifacts"
    local reconfigure=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            --install-dir=*)
                install_dir="${1#--install-dir=}"
                shift
                ;;
            --reconfigure)
                reconfigure=true
                shift
                ;;
            -h|--help)
                echo "star install - 安装产物到本地目录（DESTDIR）"
                echo "用法:"
                echo "  star install [--build-dir=<dir>] [--install-dir=<dir>] [--reconfigure]"
                echo ""
                echo "示例:"
                echo "  star install"
                echo "  star install --install-dir=artifacts"
                return 0
                ;;
            *)
                log_error "install 不支持参数: $1"
                return 1
                ;;
        esac
    done

    if [[ "$install_dir" != /* ]]; then
        install_dir="${PROJECT_ROOT}/${install_dir}"
    fi

    if ! command -v meson >/dev/null 2>&1; then
        log_error "未找到 meson，请先安装: sudo apt-get install -y meson ninja-build"
        return 1
    fi

    if [ "$reconfigure" = true ] || [ ! -d "$build_dir" ]; then
        meson setup "$build_dir" --reconfigure -Dprefix=/ -Dbindir=bin -Dlibdir=lib
    else
        meson setup "$build_dir" --reconfigure -Dprefix=/ -Dbindir=bin -Dlibdir=lib
    fi

    meson compile -C "$build_dir"
    rm -rf "$install_dir"
    meson install -C "$build_dir" --destdir "$install_dir"
    mkdir -p "$install_dir/logs"
    log_info "installed to: $install_dir"
}

run_native_runner() {
    local build_dir="${PROJECT_ROOT}/build"
    local install_dir=""
    local frames=""
    local sleep_ms=""
    local log_file=""
    local log_dir=""
    local inputs=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            --install-dir=*)
                install_dir="${1#--install-dir=}"
                shift
                ;;
            --frames=*)
                frames="$1"
                shift
                ;;
            --sleep-ms=*)
                sleep_ms="$1"
                shift
                ;;
            --log-file=*)
                log_file="${1#--log-file=}"
                shift
                ;;
            --log-dir=*)
                log_dir="${1#--log-dir=}"
                shift
                ;;
            -h|--help)
                echo "star run - 运行 native runner（Meson 构建产物）"
                echo "用法:"
                echo "  star run [--build-dir=<dir>] [--install-dir=<dir>] [--frames=<n>] [--sleep-ms=<ms>] [--log-file=<path>|--log-dir=<dir>] [--multi-process] <module|module.conf|component.conf|dir>..."
                echo ""
                echo "示例:"
                echo "  star run planning"
                echo "  star run --frames=10 modules/planning/module.conf"
                echo "  star run modules/setting/config/component.conf"
                echo "  star run --install-dir=artifacts planning"
                echo "  star run --multi-process planning"
                return 0
                ;;
            *)
                inputs+=("$1")
                shift
                ;;
        esac
    done

    if [ -n "$install_dir" ] && [[ "$install_dir" != /* ]]; then
        install_dir="${PROJECT_ROOT}/${install_dir}"
    fi

    if [ ${#inputs[@]} -eq 0 ]; then
        log_error "native-run 缺少输入（module.conf/component.conf/目录）"
        return 1
    fi

    local runner=""
    if [ -n "$install_dir" ]; then
        runner="${install_dir}/bin/native_runner"
    else
        runner="${build_dir}/apps/native_runner/native_runner"
    fi
    if [ ! -x "$runner" ]; then
        log_error "找不到 native runner: ${runner}"
        if [ -n "$install_dir" ]; then
            log_error "请先执行: star install"
        else
            log_error "请先执行: star build"
        fi
        return 1
    fi

    local runner_args=()
    if [ -n "$frames" ]; then
        runner_args+=("$frames")
    fi
    if [ -n "$sleep_ms" ]; then
        runner_args+=("$sleep_ms")
    fi

    if [ -z "$log_file" ] && [ -z "$log_dir" ] && [ -n "$install_dir" ]; then
        log_dir="${install_dir}/logs"
    fi
    if [ -n "$log_file" ]; then
        runner_args+=("--log-file=$log_file")
    elif [ -n "$log_dir" ]; then
        if [[ "$log_dir" != /* ]]; then
            log_dir="${PROJECT_ROOT}/${log_dir}"
        fi
        mkdir -p "$log_dir"
        local ts=""
        ts="$(date +%Y%m%d_%H%M%S)"
        runner_args+=("--log-file=${log_dir}/starletto_${ts}.log")
    fi
    runner_args+=("${inputs[@]+"${inputs[@]}"}")
    exec "$runner" "${runner_args[@]+"${runner_args[@]}"}"
}

run_dispatch() {
    local native_args=()
    local install_dir=""
    local build_dir=""
    for a in "$@"; do
        case "$a" in
            --install-dir=*)
                install_dir="${a#--install-dir=}"
                native_args+=("$a")
                ;;
            --build-dir=*)
                build_dir="${a#--build-dir=}"
                native_args+=("$a")
                ;;
            --frames=*|--sleep-ms=*|--log-file=*|--log-dir=*|--transport=*|--multi-process|-h|--help)
                native_args+=("$a")
                ;;
            *)
                local resolved=""
                local root="${PROJECT_ROOT}"
                if [ -n "$install_dir" ]; then
                    root="$install_dir"
                fi
                resolved="$(resolve_native_input "$a" "$root" || true)"
                if [ -z "$resolved" ]; then
                    log_error "无法解析目标: $a"
                    return 1
                fi
                native_args+=("$resolved")
                ;;
        esac
    done

    if [ -n "$install_dir" ] && [[ "$install_dir" != /* ]]; then
        install_dir="${PROJECT_ROOT}/${install_dir}"
    fi

    run_native_runner "${native_args[@]+"${native_args[@]}"}"
}

invoke_runner() {
    local build_dir="${PROJECT_ROOT}/build"
    local install_dir=""
    local runner=""
    local mode_args=()
    local paths=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            --install-dir=*)
                install_dir="${1#--install-dir=}"
                shift
                ;;
            --list|--doctor|--graph=*|--deep)
                mode_args+=("$1")
                shift
                ;;
            -h|--help)
                mode_args+=("$1")
                shift
                ;;
            *)
                paths+=("$1")
                shift
                ;;
        esac
    done

    if [ -n "$install_dir" ] && [[ "$install_dir" != /* ]]; then
        install_dir="${PROJECT_ROOT}/${install_dir}"
    fi

    if [ -n "$install_dir" ]; then
        runner="${install_dir}/bin/native_runner"
    else
        runner="${build_dir}/apps/native_runner/native_runner"
    fi

    if [ ! -x "$runner" ]; then
        log_error "找不到 native runner: ${runner}"
        if [ -n "$install_dir" ]; then
            log_error "请先执行: star install"
        else
            log_error "请先执行: star build"
        fi
        return 1
    fi

    local root="${PROJECT_ROOT}"
    if [ -n "$install_dir" ]; then
        root="$install_dir"
    fi

    local resolved_paths=()
    if [ ${#paths[@]} -eq 0 ] && [ -n "$install_dir" ]; then
        resolved_paths+=("${install_dir}/modules")
    else
        for p in "${paths[@]+"${paths[@]}"}"; do
            local resolved=""
            resolved="$(resolve_native_input "$p" "$root" || true)"
            if [ -n "$resolved" ]; then
                resolved_paths+=("$resolved")
            else
                resolved_paths+=("$p")
            fi
        done
    fi

    exec "$runner" "${mode_args[@]+"${mode_args[@]}"}" "${resolved_paths[@]+"${resolved_paths[@]}"}"
}

run_parameter_tool() {
    local build_dir="${PROJECT_ROOT}/build"
    local install_dir=""
    local tool=""
    local args=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            --install-dir=*)
                install_dir="${1#--install-dir=}"
                shift
                ;;
            -h|--help)
                echo "star param - typed parameter service 工具"
                echo "用法:"
                echo "  star param [--build-dir=<dir>|--install-dir=<dir>] demo [--format=plain|json]"
                echo "  star param [--build-dir=<dir>|--install-dir=<dir>] list [--prefix=<prefix>] [--format=plain|json] [--config-dir=<dir>]"
                echo "  star param [--build-dir=<dir>|--install-dir=<dir>] get --key=<key> [--format=plain|json] [--config-dir=<dir>]"
                echo "  star param [--build-dir=<dir>|--install-dir=<dir>] set --key=<key> --value-file=<file> [--schema=<schema>] [--format=plain|json] [--config-dir=<dir>]"
                return 0
                ;;
            *)
                args+=("$1")
                shift
                ;;
        esac
    done

    if [ -n "$install_dir" ] && [[ "$install_dir" != /* ]]; then
        install_dir="${PROJECT_ROOT}/${install_dir}"
    fi

    if [ -n "$install_dir" ]; then
        tool="${install_dir}/bin/parameter_service_demo"
    else
        tool="${build_dir}/apps/parameter_service_demo/parameter_service_demo"
    fi

    if [ ! -x "$tool" ]; then
        log_error "找不到 parameter service tool: ${tool}"
        if [ -n "$install_dir" ]; then
            log_error "请先执行: star install"
        else
            log_error "请先执行: star build"
        fi
        return 1
    fi

    exec "$tool" "${args[@]+"${args[@]}"}"
}

run_schema_tool() {
    local tool="${PROJECT_ROOT}/basement/schema/schema_codegen.py"
    local args=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|--help)
                echo "star schema - schema 工具"
                echo "用法:"
                echo "  star schema fmt --schema=<file> [--output=<file>|--in-place]"
                echo "  star schema gen --schema=<file> --output=<file>"
                echo "  star schema compat <old.schema> <new.schema>"
                echo "  star schema lint --schema=<file>"
                return 0
                ;;
            *)
                args+=("$1")
                shift
                ;;
        esac
    done

    if [ ! -f "$tool" ]; then
        log_error "找不到 schema 工具: ${tool}"
        return 1
    fi

    exec python3 "$tool" "${args[@]+"${args[@]}"}"
}

run_record_replay_tool() {
    local build_dir="${PROJECT_ROOT}/build"
    local tool="${build_dir}/apps/record_replay_tool/record_replay_tool"
    if [ ! -x "$tool" ]; then
        log_error "找不到 record_replay_tool，请先执行: star build"
        exit 1
    fi
    exec "$tool" "$@"
}

run_unit_tests() {
    local build_dir="${PROJECT_ROOT}/build"

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            -h|--help)
                echo "star test - 运行单元测试（Meson test）"
                echo "用法:"
                echo "  star test [--build-dir=<dir>]"
                echo ""
                echo "示例:"
                echo "  star test"
                echo "  star test --build-dir=build"
                return 0
                ;;
            *)
                log_error "test 不支持参数: $1"
                return 1
                ;;
        esac
    done

    if ! command -v meson >/dev/null 2>&1; then
        log_error "未找到 meson，请先安装: sudo apt-get install -y meson ninja-build"
        return 1
    fi

    if [ ! -d "$build_dir" ]; then
        log_error "build 目录不存在: $build_dir，请先执行: star build"
        return 1
    fi

    meson test -C "$build_dir"
}

run_transport_selftest() {
    local build_dir="${PROJECT_ROOT}/build"
    local install_dir=""
    local runner=""
    local args=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            --install-dir=*)
                install_dir="${1#--install-dir=}"
                shift
                ;;
            -h|--help)
                echo "star selftest - transport 自检"
                echo "用法:"
                echo "  star selftest [--build-dir=<dir>] [--install-dir=<dir>] [--transport=shm_routed]"
                echo ""
                echo "示例:"
                echo "  star selftest"
                echo "  star selftest --transport=shm_routed"
                return 0
                ;;
            *)
                args+=("$1")
                shift
                ;;
        esac
    done

    if [ -n "$install_dir" ] && [[ "$install_dir" != /* ]]; then
        install_dir="${PROJECT_ROOT}/${install_dir}"
    fi

    if [ -n "$install_dir" ]; then
        runner="${install_dir}/bin/native_runner"
    else
        runner="${build_dir}/apps/native_runner/native_runner"
    fi

    if [ ! -x "$runner" ]; then
        log_error "找不到 native runner: ${runner}"
        if [ -n "$install_dir" ]; then
            log_error "请先执行: star install"
        else
            log_error "请先执行: star build"
        fi
        return 1
    fi

    exec "$runner" --selftest-transport "${args[@]+"${args[@]}"}"
}

# 校验 schema 生成物没有回到源码树。
# codegen 已由 custom_target 接管（.schema -> .s.h 输出到 build dir），漂移在结构上
# 不可能再发生；此处防止有人把生成物重新提交进仓库，导致源/生成物再次分叉。
check_schema_generated_untracked() {
    local found
    found=$(find "${PROJECT_ROOT}" -name '*.s.h' -not -path '*/build/*' -not -path '*/.git/*' 2>/dev/null)
    if [ -n "$found" ]; then
        log_error "schema 生成物不应位于源码树（应由 custom_target 生成到 build dir）:"
        printf '%s\n' "$found" | sed 's/^/    /'
        return 1
    fi
    return 0
}

# schema lint：结构检查（字段 id 重复/非法、字段名重复、root 存在性）
check_schema_lint() {
    local tool="${PROJECT_ROOT}/basement/schema/schema_codegen.py"
    local failed=0
    local f
    while IFS= read -r f; do
        if ! python3 "$tool" lint --schema "$f" >/dev/null 2>&1; then
            log_error "schema lint 失败: ${f#"${PROJECT_ROOT}"/}"
            python3 "$tool" lint --schema "$f" 2>&1 | sed 's/^/    /'
            failed=1
        fi
    done < <(find "${PROJECT_ROOT}" -name '*.schema' -not -path '*/build/*' -not -path '*/.git/*' | sort)
    return $failed
}

# schema compat：以 git HEAD 为基线，改动不得破坏与已提交版本的兼容性
# （对应 spec 的演进禁令：改字段 id、类型互换、删字段等）
check_schema_compat() {
    local tool="${PROJECT_ROOT}/basement/schema/schema_codegen.py"
    local failed=0
    local rel full tmp
    tmp=$(mktemp -d) || return 1
    while IFS= read -r rel; do
        [ -n "$rel" ] || continue
        full="${PROJECT_ROOT}/${rel}"
        [ -f "$full" ] || continue
        git -C "${PROJECT_ROOT}" show "HEAD:${rel}" > "${tmp}/old.schema" 2>/dev/null || continue
        if ! python3 "$tool" compat "${tmp}/old.schema" "$full" >/dev/null 2>&1; then
            log_error "schema 兼容性检查失败: ${rel}"
            python3 "$tool" compat "${tmp}/old.schema" "$full" 2>&1 | sed 's/^/    /'
            failed=1
        fi
    done < <(git -C "${PROJECT_ROOT}" diff --name-only HEAD -- '*.schema')
    rm -rf "$tmp"
    return $failed
}

run_ci_guard() {
    local build_dir="${PROJECT_ROOT}/build"
    local install_dir=""
    local targets=()
    local verbose=false
    local skip_build=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir=*)
                build_dir="${1#--build-dir=}"
                shift
                ;;
            --install-dir=*)
                install_dir="${1#--install-dir=}"
                shift
                ;;
            --verbose|-v)
                verbose=true
                shift
                ;;
            --skip-build)
                skip_build=true
                shift
                ;;
            -h|--help)
                echo "star check - CI Guard: 构建后自动化验证"
                echo "用法:"
                echo "  star check [--build-dir=<dir>] [--install-dir=<dir>] [--verbose] [--skip-build] [目标...]"
                echo ""
                echo "验证项:"
                echo "  1. build      — 编译产物存在且可执行"
                echo "  2. schema     — 生成物未入源码树 + lint + 与 HEAD 的兼容性检查"
                echo "  3. unit-test  — 单元测试全部通过"
                echo "  4. selftest   — transport 自检通过"
                echo "  5. doctor     — 模块/组件清单诊断通过"
                echo "  6. list       — 组件列表输出正常"
                echo ""
                echo "示例:"
                echo "  star check                    # 全量检查（默认 planning 模块）"
                echo "  star check planning           # 检查指定模块"
                echo "  star check --verbose          # 详细输出每步结果"
                echo "  star check --skip-build       # 跳过编译步骤"
                return 0
                ;;
            *)
                targets+=("$1")
                shift
                ;;
        esac
    done

    if [ -n "$install_dir" ] && [[ "$install_dir" != /* ]]; then
        install_dir="${PROJECT_ROOT}/${install_dir}"
    fi

    local runner=""
    if [ -n "$install_dir" ]; then
        runner="${install_dir}/bin/native_runner"
    else
        runner="${build_dir}/apps/native_runner/native_runner"
    fi

    local pass_count=0
    local fail_count=0
    local total=0

    _check_step() {
        total=$((total + 1))
        local name="$1"
        shift
        if [ "$verbose" = true ]; then
            log_info "  [$name] $*"
        fi
        if "$@"; then
            pass_count=$((pass_count + 1))
            [ "$verbose" = true ] && log_info "  ✅ PASS: $name"
            return 0
        else
            fail_count=$((fail_count + 1))
            log_error "  ❌ FAIL: $name"
            return 1
        fi
    }

    if [ "$skip_build" != true ]; then
        log_info "--- CI Guard: Step 1/8 Build ---"
        if _check_step "build" native_build; then
            :
        else
            log_error "CI Guard: 构建失败，终止验证"
            exit 1
        fi
    else
        log_info "--- CI Guard: 跳过构建 ---"
    fi

    log_info "--- CI Guard: Step 2/8 Schema ---"
    _check_step "schema-clean" check_schema_generated_untracked
    _check_step "schema-lint" check_schema_lint
    _check_step "schema-compat" check_schema_compat

    log_info "--- CI Guard: Step 3/8 Unit Test ---"
    _check_step "unit-test" meson test -C "$build_dir"

    log_info "--- CI Guard: Step 4/8 Selftest ---"
    # 注意：不能调用 run_transport_selftest，它内部用 exec 会替换当前进程
    _check_step "selftest" "$runner" --selftest-transport

    log_info "--- CI Guard: Step 5/8 Doctor ---"
    if [ ${#targets[@]} -eq 0 ]; then
        for mod_dir in "${PROJECT_ROOT}"/modules/*/; do
            if [ -d "$mod_dir" ] && [ -f "${mod_dir}module.conf" ]; then
                _check_step "doctor-$(basename "$mod_dir")" "$runner" --doctor "$mod_dir"
            fi
        done
    else
        for t in "${targets[@]+"${targets[@]}"}"; do
            _check_step "doctor-$t" "$runner" --doctor "$t"
        done
    fi

    log_info "--- CI Guard: Step 6/8 List ---"
    if [ ${#targets[@]} -eq 0 ]; then
        _check_step "list-all" "$runner" --list
    else
        for t in "${targets[@]+"${targets[@]}"}"; do
            _check_step "list-$t" "$runner" --list "$t"
        done
    fi

    log_info ""
    log_info "========================================="
    log_info "CI Guard 结果: $pass_count/$total PASS, $fail_count FAIL"
    if [ "$fail_count" -gt 0 ]; then
        log_error "========================================="
        return 1
    fi
    log_info "========================================="
    return 0
}

# 处理命令
case "${1:-}" in
    build)
        shift
        native_build "$@"
        ;;
    clean)
        shift
        clean_native "$@"
        ;;
    install)
        shift
        install_native "$@"
        ;;
    run)
        shift
        run_dispatch "$@"
        ;;
    list)
        shift
        invoke_runner --list "$@"
        ;;
    graph)
        shift
        graph_format="module-dot"
        args=()
        for a in "$@"; do
            if [ "$a" = "--components" ]; then
                graph_format="component-dot"
                continue
            fi
            args+=("$a")
        done
        invoke_runner --graph="$graph_format" "${args[@]+"${args[@]}"}"
        ;;
    doctor)
        shift
        invoke_runner --doctor "$@"
        ;;
    selftest)
        shift
        run_transport_selftest "$@"
        ;;
    param)
        shift
        run_parameter_tool "$@"
        ;;
    schema)
        shift
        run_schema_tool "$@"
        ;;
    record)
        shift
        run_record_replay_tool "$@"
        ;;
    replay)
        shift
        run_record_replay_tool replay "$@"
        ;;
    monitor)
        shift
        invoke_runner --monitor "$@"
        ;;
    check)
        shift
        run_ci_guard "$@"
        ;;
    test)
        shift
        run_unit_tests "$@"
        ;;
    native-build)
        shift
        native_build "$@"
        ;;
    help)
        show_help
        exit 0
        ;;
    *)
        log_error "未知命令: ${1:-}"
        show_help
        exit 1
        ;;
esac
