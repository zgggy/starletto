#!/usr/bin/env python3
"""安装后清理 include 树：只保留 .h/.hpp。"""

from __future__ import annotations

import os
import pathlib
import sys

prefix = pathlib.Path(os.environ.get("MESON_INSTALL_DESTDIR_PREFIX", ""))
if not prefix:
    prefix = pathlib.Path(os.environ.get("DESTDIR", "")) / "usr/local"
target = prefix / sys.argv[1] if len(sys.argv) > 1 else prefix
if not target.exists():
    raise SystemExit(0)

for path in sorted(target.rglob("*"), key=lambda item: len(item.parts), reverse=True):
    if path.is_file() and path.suffix not in {".h", ".hpp"}:
        path.unlink()

for path in sorted(target.rglob("*"), key=lambda item: len(item.parts), reverse=True):
    if path.is_dir() and not any(path.iterdir()):
        path.rmdir()
