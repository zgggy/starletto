#!/usr/bin/env python3
# task_sync.py - 任务同步脚本
# 用途：从代码变更 / spec 变更自动同步任务状态

import os
import re
from datetime import datetime
from pathlib import Path


HUB_ROOT = Path(__file__).parent.parent.parent.parent
TASKS_PENDING = HUB_ROOT / "02_tasks/01_pending/backlog.md"
TASKS_ACTIVE = HUB_ROOT / "02_tasks/02_active/tracking.md"


def extract_completed_features() -> list[str]:
    """从 changelog 提取已完成的功能 ID"""
    changelog = HUB_ROOT / "03_dev/04_release/changelog.md"
    if not changelog.exists():
        return []
    text = changelog.read_text()
    pattern = r'\*\*(F\d+|B\d+|T\d+|P\d+|M\d+)\*\*'
    return re.findall(pattern, text)


def main():
    completed = extract_completed_features()
    print(f"[I] found {len(completed)} completed items: {', '.join(completed)}")

    for item in completed:
        print(f"[I] {item} - consider archiving to 03_archived/")


if __name__ == "__main__":
    main()
