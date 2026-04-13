# Basement / Schema 设计方案（.schema + 值文件 + Codec + Zero-copy）

这里不再给这套体系单独起品牌名。  
它就是 Starletto 的 **schema 体系**：`.schema` 类型定义、值文件、codegen、compat、codec、运行时解析与调用的整套逻辑。

本文档定义这套 schema 体系的能力边界、核心概念、IDL 语法、演进规则、codegen 与运行时接口。

## 0. 文件类型与职责

- `.schema`：类型定义文件（IDL）
  - 只描述结构：`schema/root/def`、字段、类型、可选、默认值、field id
  - 例：`basement/schema/builtin/manifest/module.schema`
- `.s.h`：由 `.schema` 生成的 C++ 头文件
  - 包含：TypeId/Descriptor、binary codec glue、View/Builder 接口
  - 例：`modules/setting/schema/planning/motion.s.h`
- 值文件：debug_text codec 的人类可编辑表示
  - 例：`modules/planning/module.conf`、`modules/planning/motion/component.conf`

核心原则：`.schema` 定义类型；值文件承载值；`.s.h` 是编译期生成物。

## 1. 目标

### 1.1 对齐 protobuf 的能力

- IDL：message/enum/oneof/map/repeated/optional/default
- 字段 id：稳定 tag
- 二进制 wire format：确定性、跨平台一致、可流式解析
- unknown field 保留
- 兼容演进规则与检查工具
- 反射与 descriptor

### 1.2 超过 protobuf 的能力

- zero-copy first：生成 `View/Builder`，服务 shm loan 场景
- 多 codec 后端：同一 IDL 支持 `binary/debug_text/json`
- 观测一体化：可选 trace/meta，服务延迟分析与回放一致性

## 2. 核心概念与术语

- **TypeId**：`(name, version, hash)`
- **field id**：字段编号，是 wire format 的 tag 来源
- **wire format**：消息传输时真实的 bytes 编码规则
- **View**：对 bytes 的只读结构化访问
- **Builder**：在可写 buffer 上构造消息
- **Loan**：从 transport 借一段 buffer，写完 commit
- **Unknown field**：当前版本不认识但需要保留的字段

## 3. Schema 模块拆分

继续沿用 `basement/schema` 目录，子模块如下：

- `schema/idl`：IDL 解析器、AST、格式化器、compat 规则
- `schema/descriptor`：TypeDescriptor/FieldDescriptor/EnumDescriptor/OneofDescriptor
- `schema/codec`：binary codec、debug_text/json
- `schema/runtime`：Registry、Reflection API、Codec vtable
- `schema/tools`：fmt/gen/compat/lint

所有新增能力都必须落在 `schema` 术语与 `basement/schema/*` 目录下。

工具入口统一为：

- `star schema fmt`
- `star schema gen`
- `star schema compat`
- `star schema lint`

## 4. IDL 语法

Schema IDL 必须同时满足：

- 人类可读
- 工具可生成稳定 field id
- 支持跨语言实现

### 4.1 设计约束

- IDL 文件必须能被稳定格式化
- TypeId.name 必须全局唯一
- field id 一旦发布不可复用
- 编码与兼容规则必须可被工具检查

### 4.2 示例：IDL

```text
schema manifest.module

root ModuleInfo {
    1 name : string
    2 version : string ?
    3 default_backend : string ?
    4 install_group : string ?
}

def LayoutInfo {
    1 config_dir : string ?
    2 component_dir : string ?
}

def DependencyInfo {
    1 modules : string[] ?
    2 libraries : string[] ?
}

def ModuleConf {
    1 module : ModuleInfo
    2 layout : LayoutInfo ?
    3 components : string[] ?
    4 dependency : DependencyInfo ?
}
```

说明：

- `.schema` 是 schema 体系的 IDL
- 当前最小变更是给字段加 field id，例如 `1 module : ModuleInfo`
- optional/required 与类型名之间必须有空格：`string ?` / `string !`
- repeated 使用 `type[]` 表达

### 4.3 示例：值文件

```text
module {
    name = planning
    version = 0.1.0
    default_backend = native
    install_group = core
}

layout {
    config_dir = config
    component_dir = .
}

components [motion/component.conf]

dependency {
    modules [setting]
    libraries [element, method]
}
```

值文件只写“值”，不得包含 `.schema` 中的类型定义语句。

### 4.4 root / def 语义

- `root` 与 `def` 都生成对应的 C++ 类型
- `root` 是该 `.schema` 的默认 entry 类型
- `def` 是可复用子类型，也允许在显式指定时作为 entry 类型

### 4.5 词法与类型系统

- 标识符：`[A-Za-z_][A-Za-z0-9_]*`
- 标量：`bool/int/uint/float/double/string/bytes`
- 容器：`T[]`、`{string : T}`
- 结构：`root/def` 定义的消息类型

## 5. Wire Format（binary.v1）

设计目标：

- 对齐 protobuf 的可扩展性：`tag = (field_id << 3) | wire_type`
- 支持 unknown field 保留
- 支持零拷贝 view
- 编码必须确定性

### 5.1 线格式

每条字段按 TLV 写入：

- `tag`：varint
- `payload`：
  - VARINT：varint
  - FIXED32/FIXED64：小端
  - BYTES：`len(varint) + bytes`

wire_type：

- 0：VARINT
- 1：FIXED64
- 2：BYTES
- 5：FIXED32

Schema v1 不使用 protobuf 的 group。

### 5.2 最小原型范围

当前 binary.v1 最小原型支持：

- `bool/int/uint/float/double/string`
- 嵌套对象
- `T[]`
- `{string : T}`

当前尚未做完：

- 完整 UnknownFieldSet
- 无 schema 的任意 object 编码
- 真正零拷贝的生成态 View/Builder 内部实现

## 6. 生成态接口

### 6.1 Descriptor / Codec Glue

每个生成的 root 类型至少提供：

- `schema_type_id()`
- `schema_descriptor()`
- `encode_binary()`
- `from_binary(bytes)`
- `register_schema(SchemaRegistry&)`

每个生成的字段至少暴露：

- `kFieldId_<field>`

### 6.2 Builder

每个生成的 root 类型至少提供：

- `XxxBuilder::FromValue(const Xxx&)`
- `XxxBuilder::EncodeTo(uint8_t* buf, size_t cap) -> size_t`
- `XxxBuilder::EncodeTo(std::span<uint8_t>) -> size_t`

### 6.3 View

每个生成的 root 类型至少提供：

- `XxxView::FromBinary(bytes)`
- `XxxView::Has_<field>()`
- `XxxView::<field>()`

## 7. Registry 与反射 API

Schema 运行时至少提供：

- `SchemaRegistry::register_type(TypeDescriptor, CodecVTable)`
- `SchemaRegistry::find(TypeId)`
- `SchemaRegistry::find_by_type_id(TypeId)`
- `Reflection`：按字段名/id 读取与写入

本轮最小目标：

- 先不做完整 `CodecVTable`
- 先打通生成态 descriptor 注册闭环

## 8. 兼容演进规则

必须实现的规则：

- 字段 id 一旦发布不可复用
- 已有字段不可无约束删除
- 字段可重命名，但 id 必须不变
- 字段类型变更必须满足兼容
- 嵌套类型递归检查
- reserved id/name 机制

必须提供工具：

- `star schema compat old.schema new.schema`

返回码：

- `0`：兼容
- `1`：存在 breaking change
- `2`：输入/解析错误

### 8.1 兼容性规则表（v1 最小表）

- 新增字段：允许
- 删除字段：允许但必须 `reserved id/name`
- 重命名字段：允许（id 不变）
- 改变字段 id：禁止
- 改变字段类型：
  - `uint32 -> uint64`：允许
  - `int32 -> int64`：允许
  - `float32 <-> float64`：禁止
  - `string <-> bytes`：禁止
  - `scalar <-> message`：禁止
- `repeated/optional` 互换：v1 禁止

### 8.2 lint 最小约束

工具入口：

- `star schema lint --schema=<file>`

返回码：

- `0`：通过
- `1`：存在 lint 错误
- `2`：输入/解析错误

最小检查项：

- `field id > 0`
- 同一消息内 field id 不重复
- 同一消息内 field name 不重复
- root 能被 `root/def` 集合解析

## 9. 与现有代码的关系

### 9.1 ConfValue 的处理

- `ConfValue` 退化为文本配置导入与 debug_text codec 细节
- 禁止继续作为 Channel/Service 的业务主载体

### 9.2 升级路径

当前已有：

- `SchemaRegistry/SchemaValidator`
- `schema_codegen.py`

升级步骤：

1) 引入 field id 与 compat 规则  
2) 新增 binary codec + View/Builder codegen  
3) 把 manifest/config/parameter/message 逐步迁移为 schema 类型

### 9.3 当前已落到主链路的部分

- manifest 解析入口已优先转为生成态 typed manifest
- `setting.parameter.snapshot` 发布生成态 `ParameterSnapshot`
- `setting.config.snapshot` 发布生成态 `ConfigSnapshot`
- `planning.motion` 已优先订阅生成态 snapshot，并直接读取 `payload_binary`
- `setting/config/files` 已达到 `binary_only=5, debug_text_only=0`
- `setting.parameter.get/set/list` 已具备 typed request/response 主路径

## 10. Schema v1 验收标准

最小验收：

- `setting.parameter.snapshot` 与 `setting.config.snapshot` 使用 schema binary + shm loan/view
- 单进程仍走指针传递，不发生序列化
- `star record/replay` 能记录并回放这两条 channel

当前最小落地约束：

- inproc：已优先切到 generated typed snapshot ptr
- shm：下一步必须切到 binary bytes + generated read path
- 兼容回退仍暂时保留：
  - `ConfValue`
  - `ShmTextView`
- `planning.motion`：
  - producer 必须写 `payload_binary`
  - consumer 不再回退 `debug_text`
- `modules/setting/config/files` 下 5 条 entry 必须全部进入 schema-known 集合
- `setting.parameter.get/set/list` 必须定义显式 request/response schema

## 11. 本轮验收命令

```bash
./star build
./star schema fmt --schema=modules/setting/schema/planning/motion.schema
./star schema gen --schema=modules/setting/schema/planning/motion.schema --output=/tmp/motion.s.h
./star schema compat /tmp/compat_old.schema /tmp/compat_new_ok.schema
./star schema lint --schema=modules/setting/schema/planning/motion.schema
./build/apps/schema_codec_demo/schema_codec_demo \
  basement/schema/builtin/manifest/module.schema \
  modules/planning/module.conf
./star run --frames=2 planning
./star run --multi-process --frames=6 planning
./star doctor --deep planning
```

## 12. Codegen 工具（schema_codegen.py）

### 12.1 目标

消除手动维护 `.s.h` 文件的痛点。`.schema` 是唯一事实源，`schema_codegen.py` 负责生成完整的 C++ 头文件。

### 12.2 IDL 语法（完整）

```
schema <type_name>              # TypeId.name
root <TypeName>                  # 根类型（可选，用于 ComponentLoadPlan 等复杂 schema）
namespace <cpp::namespace::path> # C++ 命名空间
version <n>                      # 版本号

def TypeName {                   # 结构体定义
    <field_id> field_name : Type [? | []]  # 字段：id, 名, 类型, 可选/重复
}

# 类型映射：
#   string  → std::string        FieldKind::STRING
#   int     → int64_t            FieldKind::INT64
#   bool    → bool               FieldKind::BOOL
#   double  → double             FieldKind::DOUBLE
#   Other   → 嵌套 struct       FieldKind::OBJECT
#   ?       → std::optional<>    optional=true
#   []      -> std::vector<>     repeated=true
```

### 12.3 生成物（每个 .s.h）

对每个 `def` 生成：

1. **struct TypeName** — 数据结构
   - `kFieldId_xxx` 静态常量
   - 成员变量（optional 字段用 `std::optional`）
   - `has_xxx()` 方法（仅 optional 字段）
   - `from_value(SchemaValue)` — 反序列化
   - `to_value() -> SchemaValue` — 序列化
   - root 类型额外：`encode_binary()`, `from_binary()`, `register_schema()`, `unknown_fields`

2. **struct TypeNameView** — 只读视图
   - `Has_xxx()` + `xxx()` 访问器

3. **struct TypeNameBuilder** — 构造器

4. **_build_fields_TypeName()** — FieldDescriptor 列表

5. **_schema_type_id()** / **_schema_descriptor()** — 元信息

6. **辅助模板**：`schema_read<T>()`, `schema_write<T>()`, concept 定义

### 12.4 工具接口

```bash
# 单文件生成
python3 basement/schema/schema_codegen.py gen \
    --schema modules/setting/schema/planning/motion.schema \
    --output /tmp/motion.s.h

# 批量生成（遍历目录，自动发现所有 .schema）
python3 basement/schema/schema_codegen.py gen \
    --schema-dir basement/schema \
    --output-dir basement/schema

# 格式化（标准化缩进/排序）
python3 basement/schema/schema_codegen.py fmt \
    --schema <file> [--output <file> | --in-place]

# 兼容性检查（旧 schema vs 新 schema）
python3 basement/schema/schema_codegen.py compat <old.schema> <new.schema>

# Lint 检查
python3 basement/schema/schema_codegen.py lint --schema <file>
```

### 12.5 验收标准

- ✅ **工具实现**（2026-04-06）：
  - `basement/schema/schema_codegen.py` 可执行
  - 支持 schema/root/namespace/version/def 全部语法
  - 正确处理 string/int/bool/double/object/?/[] 所有类型组合
- ✅ **全量验证**（2026-04-06）：22 个 .schema 文件均可正确生成对应 .s.h
- ✅ **集成到 build**（2026-04-06）：
  - Meson `run_command()` 在 configure 时自动调用 codegen
  - `basement/meson.build`：`--schema-dir schema/ --output-dir schema/`（4 个 builtin schema）
  - `modules/setting/meson.build`：`--schema-dir schema/ --output-dir schema/`（16 个 module schema）
  - 新增 .schema 文件无需修改 meson.build（自动发现）
- ⬜ **CI 守护**：`.s.h` 文件必须由 codegen 生成，禁止手动修改
- ✅ **CI 守护**（2026-04-07）：
  - `tools/check_schema_generated.sh`：独立验证脚本（对比 .s.h 与 codegen 输出）
  - `.git-hooks/pre-commit`：git hook（已安装到 `.git/hooks/pre-commit`），commit 时自动运行
  - 检测到 mismatch 时 block commit 并输出修复命令
  - Header guard 路径差异通过 normalize 处理（忽略绝对路径导致的 guard 名不同）

## 13. 参考

- Apollo CyberRT 术语：`/home/zgy/projects/apollo/docs/04_CyberRT/CyberRT_Terms.md`
- Apollo scheduler：`/home/zgy/projects/apollo/docs/04_CyberRT/CyberRT_Scheduler_cn.md`
