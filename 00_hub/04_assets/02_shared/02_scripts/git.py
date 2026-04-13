#!/usr/bin/env python3
# git.py - Git 工具脚本模板
# 用途：Git 操作辅助（tag、changelog 生成等）

import subprocess
import sys
from datetime import datetime


def run(cmd: list[str]) -> str:
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return result.stdout.strip()


def get_latest_tag() -> str:
    try:
        return run(["git", "describe", "--tags", "--abbrev=0"])
    except subprocess.CalledProcessError:
        return "v0.0.0"


def get_log_since_tag(tag: str) -> str:
    return run(["git", "log", f"{tag}..HEAD", "--oneline"])


def create_tag(version: str, message: str) -> None:
    run(["git", "tag", "-a", version, "-m", message])
    print(f"[I] created tag {version}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: git.py [tag|log|changelog]")
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "tag":
        tag = sys.argv[2] if len(sys.argv) > 2 else f"v0.{datetime.now().strftime('%y%m%d')}"
        create_tag(tag, f"Release {tag}")
    elif cmd == "log":
        tag = sys.argv[2] if len(sys.argv) > 2 else get_latest_tag()
        print(get_log_since_tag(tag))
    else:
        print(f"[E] unknown command: {cmd}")
        sys.exit(1)
