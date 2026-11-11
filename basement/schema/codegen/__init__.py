"""Schema 编解码工具的内部模块。

schema_codegen.py 是唯一入口（CLI）；这里只放可复用的部分：
  ir.py     — 语法树与内容哈希（无 C++ 依赖）
  parser.py — `.schema` 文本 → IR
  cpp.py    — IR → C++ 头文件（唯一的 C++ emitter）
  text.py   — IR → 规范化 `.schema` 文本（fmt 用）
"""
