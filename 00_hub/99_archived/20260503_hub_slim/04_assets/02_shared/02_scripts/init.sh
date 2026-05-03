#!/bin/bash
# init.sh - 项目初始化脚本
# 用途：首次克隆后执行的环境检查与初始化

set -euo pipefail

echo "=== Starletto Environment Check ==="

# 检查编译器
if command -v c++ &>/dev/null; then
    CXX_VER=$(c++ --version | head -1)
    echo "[OK] compiler: $CXX_VER"
else
    echo "[E] c++ not found"
    exit 1
fi

# 检查 Python
if command -v python3 &>/dev/null; then
    PY_VER=$(python3 --version)
    echo "[OK] python: $PY_VER"
else
    echo "[E] python3 not found (needed for codegen)"
    exit 1
fi

# 检查 Meson
if command -v meson &>/dev/null; then
    MESON_VER=$(meson --version)
    echo "[OK] meson: $MESON_VER"
else
    echo "[E] meson not found"
    exit 1
fi

# 检查 Ninja
if command -v ninja &>/dev/null; then
    NINJA_VER=$(ninja --version)
    echo "[OK] ninja: $NINJA_VER"
else
    echo "[E] ninja not found"
    exit 1
fi

# 设置 pre-commit hook
HOOK_PATH=".git/hooks/pre-commit"
HOOK_SRC=".git-hooks/pre-commit"
if [ -f "$HOOK_SRC" ] && [ ! -e "$HOOK_PATH" ]; then
    ln -s "../../$HOOK_SRC" "$HOOK_PATH"
    echo "[OK] pre-commit hook installed"
elif [ -e "$HOOK_PATH" ]; then
    echo "[OK] pre-commit hook already exists"
else
    echo "[W] pre-commit hook source not found: $HOOK_SRC"
fi

echo ""
echo "=== All checks passed ==="
echo "Run './star build' to start building"
