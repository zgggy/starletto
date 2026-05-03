#!/bin/bash
# deploy.sh - 部署脚本模板
# 用途：将 build artifacts 部署到目标路径

set -euo pipefail

SOURCE_DIR="${1:-build/artifacts}"
TARGET_DIR="${2:-/opt/starletto}"

if [ ! -d "$SOURCE_DIR" ]; then
    echo "[E] source dir not found: $SOURCE_DIR"
    exit 1
fi

echo "[I] deploying from $SOURCE_DIR to $TARGET_DIR"
mkdir -p "$TARGET_DIR"
cp -r "$SOURCE_DIR"/* "$TARGET_DIR"/
echo "[I] deployment done"
