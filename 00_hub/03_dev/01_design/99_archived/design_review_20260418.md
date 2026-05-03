# Starletto Basement 全量设计审计报告（2026-04-18）

**审计范围**：basement/ 全部核心模块（core / schema / transport / runtime / manifest）
**审计维度**：架构设计、概念设计、算法设计、软件设计
**审计方法**：逐文件完整阅读，对照 spec 和编码规范

---

## 〇、核心结论

**上次评估的 88% 成熟度是幻觉。实际成熟度约 65%。**

| 维度 | 上次标称值 | 实际值 | 差距原因 |
|------|-----------|--------|----------|
| 架构一致性 | 95% | 70% | 数据表示三重冗余、SHM 服务总线不可用、路由逻辑双重维护 |
| 概念完整性 | 95% | 70% | correlation_id 概念分裂、ACK/async 语义名不副实、Port 职责过重 |
| 代码可维护性 | 92% | 65% | codegen 模板代码 4 倍重复、头文件保护宏含绝对路径、命名规范大面积违反 |
| 测试覆盖 | 55% | 45% | 现有测试未覆盖 SHM 跨进程场景、FixedLayoutCodec 边界条件、并发安全 |
| **综合** | **88%** | **65%** | |

**问题统计**：

| 严重度 | 数量 | 说明 |
|--------|------|------|
| 🔴 严重 | 28 | 功能性 bug、安全漏洞、线程安全、编译失败 |
| 🟡 重要 | 46 | 设计缺陷、概念混乱、性能问题、规范违反 |
| 🟢 次要 | 23 | 命名规范、可移植性、文档缺失 |

---

## 一、🔴 严重问题（28 项，按模块分组）

### 1.1 Transport 层（16 项——问题最集中的模块）

| # | 问题 | 文件 | 影响 |
|---|------|------|------|
| T1 | **数据表示三重冗余**：MessageView/ServiceRequest/ServiceResponse 同时携带 TypedConstPtr + SchemaValue + encode_binary，调用者不知该用哪个 | message_bus.h, service_bus.h | 架构根基问题，影响所有使用者 |
| T2 | **ShmServiceBus 完全不可用**：create_server 不调用 start()，且使用未定义类型 RoutedMessageBus | shm_service_bus.h | SHM 服务调用链断裂，代码无法编译 |
| T3 | **ShmChannel 跨进程写入无互斥**：write_mutex_ 是进程内 mutex，不同进程可同时写入同一 slot | shm_channel.h | 多进程场景下数据必然损坏 |
| T4 | **ShmChannel 初始化竞态**：多进程同时 shm_open + 初始化 header，无跨进程同步 | shm_channel.h | 并发启动时读到未初始化数据 |
| T5 | **InprocAckWriter detached 线程捕获 this**：writer 销毁后线程访问悬挂指针 | native_writer.h | 段错误/UB |
| T6 | **NativeMessageBus recorder_ 数据竞争**：write_internal 在锁外访问 recorder_ | native_message_bus.h | 并发写入时可能崩溃 |
| T7 | **ShmMessageBus erase_reader 内存泄漏**：只删除 readers 不删除 reader_history | shm_message_bus.h | reader 销毁后 history 永不清理 |
| T8 | **RoutedWriter 双写 + RoutedReader 双注册 = 消息重复投递** | shm_routed_message_bus.h | 本地 reader 收到两次相同消息 |
| T9 | **compute_percentile p=0.0 bug**：ceil(0.0*n)=0，0-1 对 size_t 下溢，返回最大值而非最小值 | percentile.h | 统计数据错误 |
| T10 | **FixedLayoutHeader total_size 为 uint16_t**：单条消息不能超过 64KB | fixed_layout_codec.h | SHM 传输场景严重限制 |
| T11 | **FixedLayoutWriter 无越界检查**：index >= fixed_slots_.size() 时直接越界访问 | fixed_layout_codec.h | UB |
| T12 | **correlation_id 概念分裂**：MessageView 中为 uint64_t，ServiceRequest 中为 CorrelationId | message_bus.h, service_bus.h | 同一概念两种类型，无法互通 |
| T13 | **async_call_service 伪异步**：同步执行 handler 后立即 fulfill promise | native_service_bus.h | async 语义完全被违背 |
| T14 | **InprocAckWriter 不是真正的 ACK**：只是超时等待，从未等待 reader 确认 | native_writer.h | ACK 语义名不副实 |
| T15 | **ShmServiceBus correlation_id 滥用为状态标志**：resp.correlation_id = ok ? 0 : 1 | shm_service_bus.h | 请求响应无法配对 |
| T16 | **ShmChannel ftruncate 截断已有 shm**：打开已有共享内存时也调用 ftruncate | shm_channel.h | 可能截断其他进程正在写入的数据 |

### 1.2 Core 层（5 项）

| # | 问题 | 文件 | 影响 |
|---|------|------|------|
| C1 | **TypedConstPtr type==nullptr 绕过类型检查**：允许任意 static_pointer_cast，导致 UB | common_types.h | 安全漏洞 |
| C2 | **LoggerManager 非线程安全**：set_logger/get_logger 无同步，懒初始化可能双重创建 | logger.h | 全局基础设施在多线程下必然出问题 |
| C3 | **RollingFileLogger current_size_ 从不更新**：滚动机制完全失效 | rolling_file_logger.h | 日志文件无限增长，永不滚动 |
| C4 | **FileLogger/RollingFileLogger 日志级别重复输出**：format_message 已含级别，子类又加一层 | file_logger.h, rolling_file_logger.h | 输出格式错误 |
| C5 | **异常类命名空间不一致**：exception.h 在 star::excep，其他 core 文件在 star::base | exception.h | 模块内聚性破损 |

### 1.3 Schema 层（4 项）

| # | 问题 | 文件 | 影响 |
|---|------|------|------|
| S1 | **codegen 模板代码在每个 .s.h 中完整复制**：schema_read/schema_write/IsStdVector/IsStdMap/SchemaStructType 约 90 行重复 4 次 | 所有 .s.h | 违反 DRY，修改需重新生成所有文件 |
| S2 | **schema_bootstrap.h 硬编码依赖业务模块**：基础设施层引用 modules/setting/ 下的业务 schema | schema_bootstrap.h | 反向依赖，违反模块推进顺序 |
| S3 | **SchemaRegistry register_schema 静默覆盖**：name/key 映射可能不一致 | schema_registry.h | 多模块注册时数据不一致 |
| S4 | **SchemaRegistry find_by_name 两次查找间锁释放**：并发 register_schema 可能读到半更新状态 | schema_registry.h | 并发安全漏洞 |

### 1.4 Runtime 层（2 项）

| # | 问题 | 文件 | 影响 |
|---|------|------|------|
| R1 | **observability/channel_stats.h 命名不一致**：last_write_time_ns_ vs last_publish_time_ns_，编译错误 | runtime/observability/channel_stats.h | 编译失败 |
| R2 | **component.h std::thread.detach()**：线程生命周期不受控，组件销毁后线程可能访问悬挂状态 | component.h | 段错误/UB |

### 1.5 Manifest 层（1 项）

| # | 问题 | 文件 | 影响 |
|---|------|------|------|
| M1 | **codegen 头文件保护宏包含绝对路径**：换机器/换目录即失效 | schema_codegen.py, 所有 .s.h | 构建不可复现 |

---

## 二、🟡 重要问题（46 项，按类别分组）

### 2.1 架构设计缺陷

| # | 问题 | 模块 | 说明 |
|---|------|------|------|
| A1 | 路由逻辑双重维护 | transport | ChannelRegistry 和 ShmRoutedMessageBus 各维护一套 should_write_shm/should_read_shm |
| A2 | QoS 策略执行不一致 | transport | Native 和 SHM 对 DROP_NEWEST 处理路径不同 |
| A3 | ServiceServer 空接口 | transport | 无 start()/stop() 控制，具体实现自行添加破坏接口一致性 |
| A4 | RecordableBus 生命周期不完整 | transport | 只有 enable，缺少 disable_recorder/disable_record_channel |
| A5 | Writer::write() 返回 void | transport | 写入失败时调用者无法感知 |
| A6 | BinaryCodec 全 static 方法 | schema | 无法继承扩展或注入自定义策略 |
| A7 | BinaryCodec 无 children OBJECT 回退文本序列化 | schema | 二进制编码中嵌入文本格式，编解码路径不对称 |
| A8 | FieldDescriptor 职责过重 | schema | 同时承载元数据、类型系统、验证、序列化、嵌套结构 |
| A9 | NativeSchemaLoader 双格式解析 | schema | 新旧两种格式塞在同一个类中，格式检测逻辑脆弱 |
| A10 | Port 结构职责过重 | manifest | reader/writer/command 三种端点共用，reader 携带无意义的 ack 字段 |
| A11 | observability/channel_stats.h 与 transport/channel_stats.h 重复 | runtime | 两套独立统计实现，功能重叠 |

### 2.2 概念设计混乱

| # | 问题 | 模块 | 说明 |
|---|------|------|------|
| K1 | FieldKind::ARRAY 死枚举值 | schema | 被定义但实际语义被 repeated 标志替代，使用时抛异常 |
| K2 | explicit_required 语义未完整实现 | schema | 语法支持但解析后无效果 |
| K3 | map 的 type_name 语义不一致 | schema | 有时是类型名，有时是容器类别名 |
| K4 | type/msg_type 语义重叠 + fallback 不对称 | manifest | reader 优先 type，writer 优先 msg_type |
| K5 | SchemaValidator::validate 有修改副作用 | schema | 验证操作同时填充默认值，名字误导 |
| K6 | BugError 与 LogicError 语义重叠 | core | 注释完全相同，无清晰区分 |
| K7 | IClock::now() 语义模糊 | core | 不同实现返回不同时钟，调用者无法确定语义 |

### 2.3 线程安全问题

| # | 问题 | 模块 | 说明 |
|---|------|------|------|
| TH1 | SimulationClock 非线程安全 | core | advance()/set() 与 now() 无同步 |
| TH2 | StdoutLogger::log() 非线程安全 | core | std::cout 多次 << 非原子 |
| TH3 | LogStream 持有裸指针 | core | set_logger 替换后指针悬垂 |
| TH4 | ShmReader::poll_loop 读取环境变量 | transport | std::getenv 非线程安全 |

### 2.4 性能问题

| # | 问题 | 模块 | 说明 |
|---|------|------|------|
| P1 | estimate_payload_size 调用完整序列化 | transport | 仅用于统计却执行 encode_binary |
| P2 | BinaryCodec repeated 字段拷贝 FieldDescriptor | schema | 每个元素都拷贝含 vector 的结构体 |
| P3 | apply_qos 按值拷贝 MessageView | transport | 含 function + shared_ptr，拷贝开销大 |
| P4 | FileLogger 每次写日志都 flush | core | 高频日志场景严重影响性能 |
| P5 | compute_percentile 每次排序拷贝 | transport | O(n log n)，监控高频 channel 时瓶颈 |
| P6 | FixedLayoutCodec 编解码函数大量重复 | transport | encode/decode_object_array 等结构相同仅回调不同 |
| P7 | codegen encode_fixed_layout 与 encode_fixed_layout_into 100% 重复 | schema | 每个 struct 的序列化逻辑重复两次 |

### 2.5 错误处理缺陷

| # | 问题 | 模块 | 说明 |
|---|------|------|------|
| E1 | ShmMessageBus create_ack_writer 返回 nullptr | transport | 调用者直接使用会段错误 |
| E2 | plan_loader 错误信息丢失 | runtime | load_module 失败返回 nullopt，不携带原因 |
| E3 | plan_builder 无校验 | runtime | 静默生成带空值的 ComponentLoadPlan |
| E4 | FileLogger 构造不检查文件打开 | core | 日志被静默丢弃 |
| E5 | RollingFileLogger max_backup_files=0 下溢 | core | uint64_t(0)-1 为极大值 |
| E6 | FdGuard/FileGuard reset() 忽略关闭错误 | core | close()/fclose() 返回值被吞 |
| E7 | parse_route_plan 格式错误静默跳过 | transport | 用户无法得知配置被丢弃 |
| E8 | get_uint64 对负值静默返回 0 | schema | 非法输入不报错 |

### 2.6 代码规范违反

| # | 问题 | 模块 | 说明 |
|---|------|------|------|
| N1 | 头文件保护宏不一致 | core | 3/7 文件不遵循 __BASEMENT_* 约定 |
| N2 | 操作类函数使用 snake_case | 全局 | declare_channel/create_writer/write 等应为 CamelCase |
| N3 | View 类 Has_ 方法使用 PascalCase | schema | Has_process_group 应为 has_process_group |
| N4 | str_cat 污染公共命名空间 | core | 应放入 detail 子命名空间 |
| N5 | StarException 变参构造与 string_view 构造冲突 | core | string_view 版本对 const char* 永远不被选中 |
| N6 | StarException 不继承标准异常层次 | core | catch(runtime_error&) 无法捕获 RuntimeError |
| N7 | 硬编码字符串/数值 | 全局 | "choreography"、10秒超时、4MB SHM 容量等 |
| N8 | Logger 子类重复实现级别过滤和格式化 | core | 应在基类统一实现 |

### 2.7 兼容性与可移植性

| # | 问题 | 模块 | 说明 |
|---|------|------|------|
| X1 | typeid 跨动态库不可靠 | core | TypedConstPtr::as() 在 .so 边界可能失败 |
| X2 | schema_compat.py 与 schema_codegen.py 兼容性规则不一致 | schema | 同一变更可能被一个判定兼容另一个判定不兼容 |
| X3 | format_message 使用 POSIX 专用 localtime_r | core | 不可移植到 Windows |
| X4 | parse_default_value 使用 hack 方式调用完整解析器 | schema | 构造临时字符串启动完整解析器 |

---

## 三、🟢 次要问题（23 项，略）

包括：SchemaTypeId 缺少 operator< 和 hash 特化、parse_integer 边界处理、CorrelationId 进程重启后重复、ShmSlotView::text() 不验证 null 终止、RecordEntry 在两个文件中重复定义、ShmHeader 固定 16 元素数组浪费空间等。

---

## 四、根因分析

### 4.1 上次修复的根因是否真正消除？

| 根因 | 上次状态 | 本次发现 | 结论 |
|------|----------|----------|------|
| A（概念缺失） | ✅ 已消除 | 数据表示三重冗余(T1)、correlation_id 分裂(T12)、ACK/async 名不副实(T13/T14) | **未消除**——概念定义仍然不完整 |
| B（依赖混乱） | ✅ 已消除 | schema_bootstrap.h 反向依赖(S2)、路由逻辑双重维护(A1) | **未消除**——依赖规则仍有违反 |
| C（临时变永久） | ✅ 已消除 | codegen 模板 4 倍重复(S1)、头文件保护宏含绝对路径(M1)、parameters 文本往返 | **未消除**——临时方案仍在 |
| D（不敢重构） | ✅ 已消除 | 测试未覆盖 SHM 跨进程、并发安全、FixedLayout 边界 | **部分消除**——测试安全网不够密 |

### 4.2 新发现的根因

**根因 E：SHM 层设计时未考虑跨进程语义**

ShmChannel 的 write_mutex_ 是进程内 mutex、初始化无跨进程同步、ftruncate 截断已有 shm——这些问题的共同根源是：SHM 层被设计为"单进程内的内存映射"，而非"跨进程共享内存"。跨进程场景下的互斥、同步、一致性从未被系统性地考虑。

**根因 F：接口设计时未区分"能力"与"数据"**

MessageView/ServiceRequest 同时携带三种数据表示（TypedConstPtr + SchemaValue + encode_binary），是因为接口试图同时满足"进程内零拷贝"和"跨进程序列化"两种能力，但没有在接口层面区分这两种能力。结果是一个"万能"接口，实际上哪个能力都不能正确使用。

---

## 五、修复优先级

### P0：必须立即修复（功能性 bug / 安全漏洞 / 编译失败）

| # | 问题 | 修复方案 |
|---|------|----------|
| T2 | ShmServiceBus 不可用 | 修复 RoutedMessageBus 类型名，create_server 调用 start() |
| T3 | ShmChannel 跨进程写入无互斥 | 引入进程间 mutex（pthread_mutexattr_setpshared） |
| T4 | ShmChannel 初始化竞态 | 引入进程间同步原语保护 header 初始化 |
| T5 | InprocAckWriter detached 线程 | 改为 joinable 线程 + 析构时 join |
| T6 | recorder_ 数据竞争 | 将 recorder_ 访问纳入锁保护范围 |
| T7 | erase_reader 内存泄漏 | 同时删除 reader_history 条目 |
| T8 | 消息重复投递 | 重新设计路由策略：本地消息只走 inproc，跨组消息只走 shm |
| T9 | compute_percentile p=0.0 bug | 修复 idx 计算逻辑 |
| C1 | TypedConstPtr 安全漏洞 | type==nullptr 时抛异常或返回 nullptr |
| C2 | LoggerManager 线程安全 | 添加 mutex 保护 |
| C3 | RollingFileLogger 滚动失效 | 每次写入后更新 current_size_ |
| R1 | observability channel_stats 编译错误 | 统一命名 |

### P1：架构级修复（影响整体设计方向）

| # | 问题 | 修复方案 |
|---|------|----------|
| T1 | 数据表示三重冗余 | 重新设计为“MessageView（仅载荷）+ MessageSerializer（仅序列化能力）”；Writer/ServiceClient 显式区分 `write()` 与 `write_serialized()` 路径 |
| T12 | correlation_id 分裂 | stream/service 全部统一为 `CorrelationId`；SHM service 通过 envelope 显式承载，避免依赖 MessageView 的进程内元数据 |
| S1 | codegen 模板重复 | 提取 schema_codec_helpers.h 公共头文件 |
| M1 | 头文件保护宏绝对路径 | 改用 schema name 生成 guard |
| T10 | FixedLayoutHeader 64KB 限制 | total_size 改为 uint32_t |

### P2：设计改进（提升代码质量和可维护性）

包括：消除路由逻辑双重维护、统一 QoS 策略执行、修复 ACK/async 语义、拆分 Port 为 EndpointDescriptor、提取 codegen 公共序列化逻辑、统一命名规范等。

---

## 六、成熟度重新评估

| 维度 | 当前实际值 | P0 修复后 | P1 修复后 | 全部修复后 |
|------|-----------|----------|----------|-----------|
| 架构一致性 | 70% | 78% | 90% | 95% |
| 概念完整性 | 70% | 75% | 88% | 95% |
| 代码可维护性 | 65% | 72% | 85% | 92% |
| 测试覆盖 | 45% | 50% | 60% | 70% |
| **综合** | **65%** | **72%** | **83%** | **90%** |
