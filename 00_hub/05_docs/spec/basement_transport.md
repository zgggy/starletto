# Transport 层架构规范

#### 文档版本

- 版本：v0.6.0
- 更新日期：2026-04-19
- 状态：已定稿

***

## 1. 设计目标

Transport 层是 Starletto 的核心通信基础设施，负责：

- **Stream 通信**：1:N 发布订阅模式，用于传感器数据、控制指令等高频流式数据
- **RPC 通信**：1:1 请求响应模式，用于服务调用、参数查询等同步交互
- **跨进程通信**：通过共享内存（SHM）实现零拷贝进程间通信

***

## 2. 核心概念

### 2.1 Channel（通道）

Channel 是逻辑上的数据传输通道，类似于 ROS2 的 Topic 或 DDS 的 DataWriter/DataReader 主题。

```cpp
struct ChannelDescriptor {
    std::string name;              // 通道名称，唯一标识
    std::string type_name;         // 类型全名
    star::schema::SchemaTypeId schema_type_id;  // Schema ID
    ChannelOptions options;         // 通道选项
};

struct ChannelOptions {
    uint32_t depth{0};             // 历史消息队列深度
    DropPolicy drop_policy{DropPolicy::KEEP_LAST};  // 队列满时丢弃策略
    bool reliable{false};         // 是否需要可靠传输（ACK）
};
```

**QoS 语义**：

- `depth > 0`：启用历史消息队列，新订阅者可获取最近 `depth` 条消息
- `drop_policy = KEEP_LAST`：队列满时丢弃最旧消息，保留最近 `depth` 条
- `drop_policy = DROP_NEWEST`：队列满时丢弃最新消息（不入队）

### 2.2 Port（端口）

Port 是 Component 的输入输出端点，用于描述 Component 与外部的连接关系：

```cpp
struct Port {
    std::string component_name;   // 所属组件名
    std::string port_name;        // 端口名
    bool is_input{true};          // true=输入，false=输出
};
```

### 2.3 通信模式

| 模式         | 语义   | Writer:Reader | 可靠性 | 典型场景       |
| ---------- | ---- | ------------- | --- | ---------- |
| **Stream** | 发布订阅 | 1:N           | 可配置 | 传感器数据、控制指令 |
| **RPC**    | 请求响应 | 1:1           | 可靠  | 服务调用、参数查询  |

***

## 3. 架构图

```mermaid
graph TB
    subgraph "API Layer"
        MB[MessageBus]
        SB[ServiceBus]
        CR[ChannelRegistry]
    end

    subgraph "Common"
        CS[ChannelStats]
        CSS[ChannelStatsSnapshot]
    end

    subgraph "Transport Implementations"
        subgraph "Native"
            NMB[NativeMessageBus]
            NW[NativeWriter]
            NR[NativeReader]
            NSW[InprocSyncWriter]
            NSB[NativeServiceBus]
        end

        subgraph "SHM"
            SMB[ShmMessageBus]
            SC[ShmChannel]
            SW[ShmWriter]
            SR[ShmReader]
        end

        subgraph "Routed"
            SRMB[ShmRoutedMessageBus]
        end

        subgraph "Service - SHM"
            SSC[ShmServiceClient]
            SSS[ShmServiceServer]
        end
    end

    subgraph "Record & Replay"
        MR[MessageRecorder]
        MPR[MessageReplayer]
    end

    MB --> NMB
    MB --> SMB
    SRMB --> NMB
    SRMB --> SMB
    MB --> MR
    MR --> MPR

    NMB --> NW
    NMB --> NR
    NMB --> NAW

    SMB --> SW
    SMB --> SR
    SMB --> SC

    SB --> NSB
    SB --> SSC
    SSC --> SRMB
    SSS --> SRMB

    NMB --> CS
    SMB --> CS
    CS --> CSS

    CR --> SRMB
```

***

## 4. 目录结构

```
basement/transport/
├── api/
│   ├── message_bus.h         # 核心接口定义
│   ├── service_bus.h         # RPC 接口定义
│   └── channel_registry.h    # 通道注册表
├── common/
│   └── channel_stats.h       # 统计数据结构 + 计算函数
├── native/
│   ├── native_message_bus.h   # 进程内 MessageBus 实现
│   ├── native_writer.h       # 本地写入器
│   ├── native_reader.h       # 本地读取器
│   └── native_service_bus.h  # 进程内 ServiceBus 实现
├── shm/
│   ├── shm_message_bus.h     # 共享内存 MessageBus 实现
│   ├── shm_channel.h         # 共享内存环形缓冲区
│   ├── shm_views.h           # SHM 视图类型
│   ├── shm_routed_message_bus.h  # 路由消息总线
│   └── shm_service_bus.h     # SHM RPC 实现
└── record/
    ├── message_recorder.h   # 消息录制
    └── message_replayer.h    # 消息回放
```

**注意**：`ParameterStore` 已移至 `runtime/parameter/`，属于 Runtime 层概念。

***

## 5. 核心接口

### 5.1 MessageBus

```cpp
class MessageBus {
    virtual void declare_channel(const ChannelDescriptor& descriptor) = 0;
    virtual auto create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> = 0;
    virtual auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> = 0;
    virtual auto create_delivery_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<DeliveryWriter> = 0;
    virtual auto declared_channels() const -> std::vector<std::string> = 0;
    virtual auto channel_stats(const std::string& channel_name) const -> std::optional<ChannelStatsSnapshot>;
    virtual auto all_channel_stats() const -> std::vector<ChannelStatsSnapshot>;
    virtual auto peek_latest(const std::string& channel_name) const -> std::optional<MessageView>;
    virtual auto dump_stats_lines() const -> std::vector<std::string>;
};

class RecordableBus {
    virtual void enable_recorder(const std::string& file_path) = 0;
    virtual void enable_record_channel(const std::string& channel) = 0;
    virtual void disable_recorder() = 0;
    virtual void disable_record_channel(const std::string& channel) = 0;
};
```

### 5.2 Writer / DeliveryWriter

```cpp
class Writer {
    virtual auto write(const MessageView& message) -> WriteResult = 0;
    virtual auto write_serialized(const SerializedMessage& message) -> WriteResult = 0;
};

class DeliveryWriter : public Writer {
    auto write(const MessageView& message) -> WriteResult override = 0;
    auto write_serialized(const SerializedMessage& message) -> WriteResult override = 0;
    virtual auto write_with_delivery(const MessageView& message, int64_t timeout_ms)
        -> std::future<DeliveryResult> = 0;
    virtual auto write_serialized_with_delivery(const SerializedMessage& message, int64_t timeout_ms)
        -> std::future<DeliveryResult> = 0;
};

enum class WriteStatus {
    OK,
    BUS_EXPIRED,
    CHANNEL_UNKNOWN,
    CHANNEL_NOT_READY,
    SERIALIZER_REQUIRED,
    ENCODE_FAILED,
    SHM_DISABLED,
    SHM_DEGRADED,
    EXCEPTION,
};

struct WriteResult {
    WriteStatus status{WriteStatus::OK};
    bool inproc_ok{false};
    bool shm_ok{false};
    bool shm_required{false};
    std::string error_message;
};

struct MessageView {
    star::base::TypedConstPtr payload;
    star::schema::SchemaTypeId schema_type_id;
    CorrelationId correlation_id;
};

struct MessageSerializer {
    size_t payload_size_hint{0};
    std::function<std::vector<uint8_t>()> encode_binary;
    std::function<size_t(uint8_t*, size_t)> encode_into;
};

struct SerializedMessage {
    MessageView view;
    MessageSerializer serializer;
};
```

**载荷与序列化能力分离契约**：

- `MessageView` / `ServiceRequest.body` / `ServiceResponse.body` 只表示“已经存在的载荷视图”，不再混入序列化闭包
- `MessageSerializer` 只表示“如何把同一份载荷编码成二进制”，由调用者在跨进程或录制场景显式提供
- 单进程 inproc 写入允许仅传 `MessageView`，必须走指针语义
- 跨进程 SHM 写入必须调用 `write_serialized(...)` / `write_serialized_with_delivery(...)`（如需投递语义），不得依赖隐式 fallback

### 5.3 ChannelRegistry（路由统一事实源）

`ChannelRegistry` 同时支持两种配置方式：

- legacy：`configure_channel_sets(pub_csv, sub_csv)` 以“发布集合/订阅集合”决定是否走 SHM
- route plan：`configure_local_group(group)` + `configure_route_plan(plan)` 以“producer/consumer group”决定是否跨进程走 SHM

为避免路由判断在多处重复维护，提供统一的单通道路由判定：

```cpp
enum class ShmRouteMode {
    INPROC_ONLY,
    SHM_WRITE,
    SHM_READ,
    SHM_READ_WRITE,
};

auto shm_route_mode(const std::string& channel) const -> ShmRouteMode;
```
- `correlation_id` 统一为 `CorrelationId`，普通 stream 默认为 invalid；service 在 request/response envelope 中显式携带

### 5.3 Reader

```cpp
class Reader {
  public:
    using Callback = std::function<void(const MessageView&)>;
    virtual ~Reader() = default;
    virtual auto peek_latest() const -> std::optional<MessageView> { return std::nullopt; }
    virtual auto has_data() const -> bool { return false; }
};
```

### 5.4 ServiceBus

```cpp
class ServiceBus {
    virtual auto create_client(const ServiceDescriptor& descriptor)
        -> std::shared_ptr<ServiceClient> = 0;
    virtual auto create_server(const ServiceDescriptor& descriptor, Handler handler)
        -> std::shared_ptr<ServiceServer> = 0;
};
```

```cpp
class ServiceServer {
  public:
    virtual ~ServiceServer() = default;
    virtual void start() {}
    virtual void stop() {}
};
```

```cpp
struct ServiceRequest {
    MessageView body;
};

struct ServiceResponse {
    bool ok{true};
    std::string message;
    MessageView body;
};

struct SerializedServiceRequest {
    ServiceRequest request;
    MessageSerializer serializer;
};

struct SerializedServiceResponse {
    ServiceResponse response;
    MessageSerializer serializer;
};
```

### 5.5 ParameterStore

```cpp
class ParameterStore {
    virtual auto get(const std::string& key) const -> SchemaValue = 0;
    virtual void set(const std::string& key, const SchemaValue& value) = 0;
    virtual auto list(const std::string& prefix) const -> std::vector<std::string> = 0;
    virtual void flush() = 0;
};
```

***

## 6. 投递语义（Delivery）

Transport 层当前不提供独立的 ACK 通道与 reader 确认语义；可靠性以“投递完成（dispatch 完成）”作为边界 API 的语义基准。

`DeliveryWriter` 表示“投递语义写入”：

- `write_with_delivery(...)` / `write_serialized_with_delivery(...)` 返回 `future<DeliveryResult>`
- `accepted=true` 表示完成一次写入与 inproc dispatch（不等待读者侧确认）
- `timeout_ms` 当前仅作为接口保留参数（inproc 实现不使用该值）

***

## 7. 共享内存（SHM）实现

### 7.1 ShmChannel 环形缓冲区

```
+------------------------------------------+
|              ShmHeader                   |
|  magic | version | seq | notify_seq | ...|
+------------------------------------------+
|  Slot 0  |  Slot 1  |  Slot 2  | Slot 3 |
|  1MB     |   1MB     |   1MB    |  1MB   |
+------------------------------------------+
```

**特性**：

- 4 个 Slot，循环使用
- 每个 Slot 1MB（默认 4MB 总容量）
- Futex 等待/唤醒实现高效同步
- Loan/Commit 模式支持零拷贝

### 7.2 路由决策

`ChannelRegistry` 维护进程组间的路由信息：

```
格式: channel_name|producer_group|consumer_group1,consumer_group2,...
示例: /cmd_vel|planning|control,monitoring
```

**路由决策**：

- `should_write_shm(channel)`：本地有生产者，且有跨进程消费者
- `should_read_shm(channel)`：本地有消费者，且有跨进程生产者

### 7.3 SHM 失败降级

`ShmRoutedMessageBus` 在 SHM 操作失败时自动降级到 inproc-only 模式，确保进程不会因 SHM 资源不足而崩溃。

**降级策略**：

| 失败场景 | 降级行为 |
|---------|---------|
| SHM 初始化失败（构造函数） | `shm_available_ = false`，所有通道走 inproc |
| `declare_channel` SHM 失败 | 该通道标记为 degraded，后续操作跳过 SHM |
| `create_writer/reader` SHM 失败 | 该通道降级，仅使用 inproc writer/reader |
| `RoutedWriter::write` SHM 写入失败 | 释放 shm writer 引用，后续仅写 inproc |

**降级通道追踪**：

- `degraded_channels_`：记录已降级的通道名
- `shm_available_`：全局 SHM 可用性标志
- 所有降级事件通过 `logW` 记录

**RuntimeFactory 降级**：

- `STAR_TRANSPORT=shm` 模式：SHM 初始化失败时降级为 `NativeMessageBus`
- `STAR_TRANSPORT=shm_routed` 模式：由 `ShmRoutedMessageBus` 内部处理降级

### 7.4 FixedLayoutCodec（零拷贝 Wire Format）

`FixedLayoutCodec` 是 SHM 零拷贝的核心编解码器，定义在 `transport/common/fixed_layout_codec.h`。它只负责“用户 payload”的 fixed-layout 编码，不承载 transport 元数据；service 的 `correlation_id` 等元数据由上层 envelope 负责。

**内存布局**：

```
+-------------------+---------------------------+---------------------------+
| FixedLayoutHeader | FixedFieldSlot[0..N-1]    | Variable-length Data      |
| (10 bytes)        | (16 bytes each)           |                           |
+-------------------+---------------------------+---------------------------+
```

**FixedLayoutHeader**（10 bytes）：

| 字段 | 类型 | 说明 |
|------|------|------|
| magic | uint16_t | 固定 0x5354 |
| version | uint8_t | 固定 1 |
| flags | uint8_t | 保留 |
| fixed_section_size | uint16_t | 固定段总大小 |
| total_size | uint32_t | 总大小 |

**FixedFieldSlot**（16 bytes each）：

| 字段 | 类型 | 说明 |
|------|------|------|
| type_tag | uint8_t | 值类型标签 |
| data | union | bool_val / int64_val / float64_val / var_ref{offset, length} |

**类型标签**：

| 标签 | 值 | 说明 |
|------|------|------|
| kTagNull | 0 | 空值 |
| kTagBool | 1 | 布尔 |
| kTagInt64 | 2 | 64位整数 |
| kTagFloat64 | 3 | 64位浮点 |
| kTagString | 4 | 字符串（var_ref） |
| kTagObject | 5 | 嵌套对象（var_ref） |
| kTagArray | 6 | 数组（var_ref） |
| kTagMap | 7 | 映射（var_ref） |

**零拷贝路径**：

1. **写入**：`FixedLayoutWriter` 构建 payload → `serialize_into(buf, cap)` 直接写入 SHM loan buffer
2. **读取**：`FixedLayoutView(data, size)` 直接映射 SHM slot 内存，无拷贝
3. **codegen 集成**：`XxxView` 包装 `FixedLayoutView`，`XxxBuilder` 包装 `FixedLayoutWriter`
4. **能力边界**：transport 侧若需要 envelope（例如 SHM service request/response），必须在 `FixedLayoutCodec` 之外追加头部，不能把 transport 元数据塞进 schema payload 定义

***

## 8. Stream vs RPC

### 8.1 Stream（发布订阅）

```mermaid
graph LR
    W[Writer] -->|write| C1[Channel]
    W -->|write| C2[Channel]
    C1 -->|deliver| R1[Reader 1]
    C1 -->|deliver| R2[Reader 2]
    C2 -->|deliver| R3[Reader 3]
```

**特点**：

- 1 个 Writer → N 个 Reader
- 异步投递，不等待响应
- 支持 QoS（队列深度、丢弃策略）
- 可选 ACK 确认

### 8.2 RPC（请求响应）

```mermaid
sequenceDiagram
    Client->>+Server: ServiceRequest (correlation_id)
    Server-->>-Client: ServiceResponse (correlation_id)
```

**实现**：内部使用两个 Channel：

- `{service}/request`：请求通道
- `{service}/response`：响应通道

通过 `CorrelationId` 关联请求与响应。

### 8.3 Service 实现对比

| 特性                | NativeServiceBus                          | ShmServiceBus                              |
| ----------------- | ---------------------------------------- | ------------------------------------------ |
| 位置               | transport/native/native_service_bus.h     | transport/shm/shm_service_bus.h             |
| 通信方式           | 进程内直接函数调用                         | 基于 MessageBus 的 request/response 通道      |
| 跨进程支持         | ❌                                       | ✅                                         |
| 延迟               | 极低（函数调用）                           | 较高（SHM + 回调）                            |
| Record/Replay     | ❌                                       | ✅（自动通过 MessageBus 录制）                  |

***

## 9. Record & Replay

### 10.1 MessageRecorder

录制指定通道的消息到二进制文件：

```cpp
// 录制所有通道
bus->enable_recorder("/tmp/record.bin");

// 仅录制特定通道
bus->enable_record_channel("/sensor/lidar");

// 录制服务请求/响应（自动支持）
bus->enable_record_channel("/parameter/get/request");
bus->enable_record_channel("/parameter/get/response");
```

**录制格式**：`STARREC` + RecordEntry[]

```cpp
struct RecordEntry {
    int64_t timestamp_ns;
    std::string topic_name;      // 通道名称（包括 service 通道）
    std::string type_name;       // 类型名称
    std::vector<uint8_t> payload; // 二进制 payload
};
```

**Service 通道录制**：Recorder 自动支持录制任何已声明的 channel，包括：
- Stream 通道：`/sensor/lidar`, `/cmd/velocity`
- Service 请求通道：`/parameter/get/request`
- Service 响应通道：`/parameter/get/response`
- ACK 通道：`/sensor/lidar/ack`

### 10.2 MessageReplayer

回放录制的消息，用于仿真和回测。

***

## 11. ChannelRegistry 路由表

```mermaid
graph LR
    subgraph "Process A (Group: planning)"
        PA[planning.app]
    end

    subgraph "Process B (Group: control)"
        PB[control.app]
    end

    PA -->|SHM| /cmd_vel
    PB -->|SHM| /cmd_vel

    CR[ChannelRegistry] --> |route| /cmd_vel
```

**路由格式解析**：

```
输入: "/cmd_vel|planning|control,monitoring"
解析:
  - channel: "/cmd_vel"
  - producer_group: "planning"
  - consumer_groups: ["control", "monitoring"]

含义:
  - planning 进程组生产的 /cmd_vel 消息
  - 需要通过 SHM 投递到 control 和 monitoring 进程组
```

***

## 12. 命名规范

### 12.1 通道命名

| 类型     | 命名格式                    | 示例                                        |
| ------ | ----------------------- | ----------------------------------------- |
| 传感器数据  | `/sensor/{sensor_name}`    | `/sensor/lidar`, `/sensor/camera`         |
| 控制指令   | `/cmd/{module_name}`       | `/cmd/velocity`, `/cmd/trajectory`        |
| 状态反馈   | `/status/{module_name}`    | `/status/planning`, `/status/control`     |
| 服务请求   | `{service}/request`        | `/parameter/get/request`                   |
| 服务响应   | `{service}/response`       | `/parameter/get/response`                  |
| ACK 通道 | `{channel}/ack`            | `/sensor/lidar/ack`                       |

### 12.2 变量命名

| 元素                | 命名风格          | 示例                             |
| ----------------- | ------------- | ------------------------------ |
| 通道名               | snake\_case   | `channel_name`, `sensor_topic` |
| ChannelDescriptor | PascalCase    | `ChannelDescriptor`            |
| ChannelOptions    | PascalCase    | `ChannelOptions`               |
| Port              | PascalCase    | `Port`                         |
| 实现类               | PascalCase    | `NativeMessageBus`             |
| 内部变量              | snake\_case\_ | `channels_`, `readers_`        |

### 12.3 方法命名

| 对象     | 方法                    | 说明         |
| ------ | --------------------- | ---------- |
| Writer  | `write(message)`        | 写入消息       |
| DeliveryWriter | `write_with_delivery(msg, ms)` | 同步投递写入 |
| Reader  | 通过回调接收                 | 无主动方法      |
| MessageBus | `declare_channel(desc)`  | 声明通道       |
| MessageBus | `create_writer(desc)`    | 创建写入器      |
| MessageBus | `create_reader(desc, cb)` | 创建读取器      |

***

## 13. 实现差异对比

| 特性            | NativeMessageBus | ShmMessageBus | ShmRoutedMessageBus |
| ------------- | ---------------- | ------------- | ------------------- |
| 进程内通信         | ✅ 零拷贝            | ✅ 零拷贝         | ✅ 零拷贝               |
| 进程间通信         | ❌                | ✅ 零拷贝         | ✅ 零拷贝               |
| QoS 队列        | ✅                | ✅             | ✅                   |
| ACK 机制        | ✅                | ❌             | ✅ (通过 Native)       |
| 自动路由          | ❌                | ❌             | ✅                   |
| Record/Replay | ✅                | ❌             | ✅ (通过 Native)       |
| Service 支持      | ✅ (NativeServiceBus) | ❌            | ✅ (ShmServiceBus)     |
| 结构化统计         | ✅                | ✅             | ✅ (合并)              |
| peek_latest   | ✅                | ✅             | ✅ (委托)              |
| SHM 失败降级      | N/A              | ❌             | ✅ 自动降级到 inproc      |

***

## 14. 验收标准

```bash
# 编译验证
./star build

# 运行单元测试
./star test

# 验证 transport/service 契约改动未破坏基线
./star test

# 验证通道注册
./star run --doctor

# 验证 SHM 通信（多进程）
STAR_PROCESS_GROUP=planning ./star run &
STAR_PROCESS_GROUP=control ./star run &
```

***

## 15. 变更记录

| 版本     | 日期         | 变更内容                                                                                                          |
| ------ | ---------- | --------------------------------------------------------------------------------------------------------------- |
| v0.6.0 | 2026-04-19 | MessageView/ServiceRequest/ServiceResponse 重构为“载荷视图 + 显式 serializer”双对象；Writer/DeliveryWriter 新增 `write_serialized*`；SHM service 使用 envelope 显式携带 `CorrelationId`；FixedLayoutCodec 文档边界更新为“仅承载 payload” |
| v0.5.0 | 2026-04-17 | FixedLayoutCodec 零拷贝 wire format 实现；codegen 生成 View/Builder 端到端闭环；SHM 失败降级策略；publish/subscribe 统一为 write/read；Reader 实现 peek_latest/has_data；RecordableBus 从 MessageBus 分离；dump_stats_lines 降级为非纯虚；ResourceMonitor 添加 ClockFn 注入 |
| v0.4.0 | 2026-04-14 | 架构重整：提取 transport/common/ 公共模块；Reader 添加 peek_latest/has_data；MessageBus 添加结构化统计 API（channel_stats/all_channel_stats/peek_latest）；NativeServiceBus 从 runtime/factory/ 移至 transport/native/；ParameterStore 移至 runtime/parameter/（star::runtime 命名空间）；删除 is_conf_value_field hack |
| v0.3.1 | 2026-04-13 | Writer/DeliveryWriter 方法重命名 publish→write；服务通道命名统一为下划线；补充 Parameter Store、Service 实现对比文档；补充 Recorder 服务录制说明 |
| v0.3.0 | 2026-04-13 | 重构：TopicDescriptor → ChannelDescriptor，QoSPolicy → ChannelOptions，删除 EventBus，RouteEntry 重复消除                      |
| v0.2.0 | 2026-04-10 | 新增 ShmRoutedMessageBus，支持跨进程 SHM 路由                                                                     |
| v0.1.0 | 2026-04-01 | 初始版本，基础 MessageBus + ServiceBus                                                                             |
