# basement/schema —— 类型体系

`.schema` 是类型的唯一事实源；`schema_codegen.py` 是**唯一**的 schema 定义解析器，
生成 C++ 头（`*.s.h`，构建目录，不入库）。运行时只消费生成类型和
`SchemaDescriptor`，不再解析 schema 文本。

## 关键文件

| 路径 | 作用 |
|---|---|
| `schema_codegen.py` + `codegen/` | `gen/fmt/compat/lint`；多 root、import、reserved |
| `schema_registry.h` | `name#version` 多版本注册；同 key 内容必须一致 |
| `binary_codec.h` | TLV 编解码、unknown/missing/type_mismatch 策略 |
| `fixed_layout_codec.h` | 固定布局 v2（同版本、位置语义） |
| `schema_validator.h` | conf/值校验与默认值 |
| `schema_value.h` | 通用文本/值表示（`SchemaParser` 只解析 conf/值文本） |
| `native/schema_bootstrap.h` | 内置 schema 原子注册 |

## 身份与演进

- 类型身份：`SchemaTypeId{name, version, hash}`；哈希按 root 可达闭包计算。
- 通道/shm 帧携带发送方身份；接收端按 version/hash 和解码策略处理。
- 多版本是数据演进功能：同名 v1/v2 可同时注册；删字段必须递增 version 并 `reserved`。

## 常用命令

```bash
./star schema lint --schema path/to/x.schema
./star schema fmt  --schema path/to/x.schema
./star schema gen  --schema path/to/x.schema --output /tmp/x.s.h
./star schema compat old.schema new.schema
```

测试：`schema_binary_codec`、`schema_fixed_layout`、`schema_registry`、
`schema_robustness`、`schema_parser`、`schema_import`、`schema_multi_root`。
