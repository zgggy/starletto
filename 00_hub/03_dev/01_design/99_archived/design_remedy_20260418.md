# Starletto Basement 全量设计重整方案（2026-04-18）

**日期**：2026-04-18
**目标**：简洁、强大、优美
**前置文档**：[全量设计审计报告](design_review_20260418.md)

---

## 一、根因重新分析

### 1.1 旧根因的再评估

上次（04-14）识别了四个根因，声称"已消除"。实际审计证明**全部未消除**：

| 根因 | 上次结论 | 实际状态 | 证据 |
|------|----------|----------|------|
| A（概念缺失） | ✅ 已消除 | ❌ 未消除 | 数据表示三重冗余、correlation_id 分裂、ACK/async 名不副实、Port 职责过重 |
| B（依赖混乱） | ✅ 已消除 | ❌ 未消除 | schema_bootstrap.h 反向依赖业务模块、路由逻辑双重维护、NativeWriter 在全局命名空间 |
| C（临时变永久） | ✅ 已消除 | ❌ 未消除 | codegen 模板 4 倍重复、头文件保护宏含绝对路径、parameters 文本往返、ShmChannel 空壳 resolve_capacity |
| D（不敢重构） | ✅ 已消除 | ⚠️ 部分消除 | 测试未覆盖 SHM 跨进程、并发安全、FixedLayout 边界条件 |

### 1.2 新发现的根因

**根因 E：SHM 层设计时未考虑跨进程语义**

ShmChannel 的 `write_mutex_` 是进程内 mutex、初始化无跨进程同步、ftruncate 截断已有 shm、shm_unlink 缺失——共同根源是：SHM 层被设计为"单进程内的内存映射"，而非"跨进程共享内存"。跨进程场景下的互斥、同步、一致性从未被系统性地考虑。

**根因 F：接口设计时未区分"能力"与"数据"**

MessageView/ServiceRequest 同时携带 TypedConstPtr + SchemaValue + encode_binary/encode_into，是因为接口试图同时满足"进程内零拷贝"和"跨进程序列化"两种能力，但没有在接口层面区分。结果是一个"万能"接口，实际上哪个能力都不能正确使用——NativeMessageBus 要求 data 非空，ShmMessageBus 要求 encode_into/encode_binary 非空，但接口层没有任何表达。

### 1.3 根因之间的因果链

```
F（能力与数据未分离）→ E（跨进程语义缺失）→ A（概念缺失）
     ↑                                              │
     └──────── A 导致 F 无法被正确修正 ←─────────────┘

A（概念缺失）→ B（依赖混乱）→ C（临时变永久）
     ↑                              │
     └──── 无法验证，无法修正 ←───────┘
```

**核心洞察**：根因 F 是最深层的——因为"消息是什么"和"消息能怎么被传输"没有分离，所以 SHM 层无法正确设计跨进程语义（E），概念集永远无法完备（A），依赖关系永远无法理清（B），临时方案永远无法消除（C）。

**破局点**：必须从 F 入手——先分离"载荷"与"序列化能力"，再修复 SHM 跨进程语义，然后才能系统性地消除其余问题。

---

## 二、问题间关联矩阵

|       | F-数据表示 | E-SHM跨进程 | T2-ServiceBus | T8-消息重复 | T1-三重冗余 | S1-codegen重复 | C1-类型安全 | R2-线程管理 |
|-------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| F     | -   | →   | →   | →   | →   |     | →   |     |
| E     | ←   | -   | →   | →   |     |     |     |     |
| T2    | ←   | ←   | -   |     |     |     |     |     |
| T8    | ←   | ←   |     | -   |     |     |     |     |
| T1    | ←   |     |     |     | -   |     |     |     |
| S1    |     |     |     |     |     | -   |     |     |
| C1    | ←   |     |     |     |     |     | -   |     |
| R2    |     |     |     |     |     |     |     | -   |

---

## 三、P0 修复方案（功能性 bug / 安全漏洞 / 编译失败）

### 3.1 T2：ShmServiceBus 完全不可用

**问题**：
1. `RoutedMessageBus` 类型未定义（应为 `ShmRoutedMessageBus`），编译失败
2. `create_server` 不调用 `start()`，服务端永远不监听
3. `correlation_id` 被滥用为状态标志（0=成功，1=失败），请求响应无法配对
4. 硬编码 10 秒超时
5. `ShmServiceClient::available()` 硬编码返回 true
6. `response_readers_` 从不清理（内存泄漏）

**修复方案**：

```cpp
// 1. 类型名修正
using RoutedMessageBus = ShmRoutedMessageBus;

// 2. create_server 自动启动
auto create_server(const ServiceDescriptor& descriptor, Handler handler) -> std::shared_ptr<ServiceServer> override {
    auto server = std::make_shared<ShmServiceServer>(bus_, descriptor.name, std::move(handler));
    server->start();
    return server;
}

// 3. correlation_id 保留原始值
void handle_request(const MessageView& msg) {
    const auto request_cid = msg.correlation_id;  // 保存原始 ID
    // ... handler 执行 ...
    resp_msg.correlation_id = request_cid;  // 保留原始 ID，而非 0/1
}

// 4. 超时可配置
struct ServiceDescriptor {
    std::string name;
    int64_t timeout_ms{5000};
};

// 5. available() 检查实际状态
auto available() const -> bool override {
    return bus_ != nullptr && bus_->shm_available();
}

// 6. response_readers_ 清理
~ShmServiceClient() {
    for (auto& [_, reader] : response_readers_) {
        bus_->erase_reader(response_topic_, reader->id());
    }
}
```

**验收**：`./star build` 编译通过 + ShmServiceBus 单元测试（创建 server → client call → 收到正确响应）

---

### 3.2 T3+T4：ShmChannel 跨进程互斥与初始化竞态

**问题**：
1. `write_mutex_` 是进程内 mutex，不同进程可同时写入同一 slot
2. 多进程同时 `shm_open` + 初始化 header，无跨进程同步
3. `ftruncate` 对已有 shm 也执行，可能截断正在使用的数据
4. `shm_unlink` 缺失，/dev/shm 残留

**修复方案**：

```cpp
// ShmHeader 新增字段
struct ShmHeader {
    uint64_t magic;
    uint32_t version;
    uint32_t header_bytes;
    std::atomic<uint64_t> seq;
    std::atomic<uint32_t> notify_seq;
    std::atomic<uint32_t> write_lock;    // 新增：进程共享自旋锁
    std::atomic<uint32_t> init_state;    // 新增：0=初始化中, 1=就绪
    std::atomic<uint32_t> ref_count;     // 新增：引用计数
    uint32_t slot_size;
    uint32_t slot_count;
    uint32_t capacity;
    uint32_t reserved{0};
    std::atomic<uint32_t> sizes[kShmMaxSlots];
    std::atomic<uint64_t> write_ns[kShmMaxSlots];
};

// loan() 中替换 write_mutex_：
auto loan(uint32_t size) -> Loan {
    spin_lock(&header_->write_lock);  // 跨进程自旋锁
    // ... 选择 slot ...
    // 注意：Loan 持有锁，commit() 或析构时释放
}

// 构造函数中修复初始化竞态：
if (created) {
    header_->init_state.store(0, std::memory_order_release);
    // ... ftruncate + mmap + 初始化所有字段 ...
    header_->init_state.store(1, std::memory_order_release);
} else {
    // 等待创建者完成初始化
    for (int i = 0; i < kInitRetryCount; ++i) {
        if (header_->init_state.load(std::memory_order_acquire) == 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ftruncate 仅在创建时执行：
if (created) {
    if (::ftruncate(fd_guard_.get(), static_cast<off_t>(map_size)) != 0) {
        throw RuntimeError("ftruncate failed: ", name_);
    }
} else {
    struct stat st{};
    if (::fstat(fd_guard_.get(), &st) != 0) {
        throw RuntimeError("fstat failed: ", name_);
    }
    if (static_cast<size_t>(st.st_size) != map_size) {
        throw RuntimeError("ShmChannel size mismatch: ", name_);
    }
}

// 引用计数 + shm_unlink：
// 构造函数末尾：
header_->ref_count.fetch_add(1, std::memory_order_acq_rel);

// 析构函数：
~ShmChannel() {
    if (header_ != nullptr) {
        const auto remaining = header_->ref_count.fetch_sub(1, std::memory_order_acq_rel);
        if (remaining == 1) {
            ::shm_unlink(name_.c_str());
        }
    }
}
```

**spin_lock 实现**：

```cpp
static void spin_lock(std::atomic<uint32_t>* lock) {
    while (lock->exchange(1, std::memory_order_acquire) != 0) {
        // 短暂自旋后调用 futex 等待
        if (lock->load(std::memory_order_relaxed) == 0) continue;
        syscall(SYS_futex, lock, FUTEX_WAIT, 1, nullptr, nullptr, 0);
    }
}

static void spin_unlock(std::atomic<uint32_t>* lock) {
    lock->store(0, std::memory_order_release);
    syscall(SYS_futex, lock, FUTEX_WAKE, 1, nullptr, nullptr, 0);
}
```

**验收**：`./star build` + 双进程 SHM 写入测试（两个进程同时写入同一 channel，数据不损坏）

---

### 3.3 T5：InprocAckWriter detached 线程

**问题**：`write_with_ack` 创建 detached 线程捕获 `this`，writer 销毁后线程访问悬挂指针

**修复方案**：

```cpp
class InprocAckWriter : public Writer {
public:
    // ...

    ~InprocAckWriter() {
        // 等待所有 ack 线程完成
        std::vector<std::future<void>> pending;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending = std::move(pending_acks_);
        }
        for (auto& f : pending) {
            if (f.valid()) { f.wait(); }
        }
    }

    void write_with_ack(const MessageView& message, uint64_t timeout_ms) override {
        auto promise = std::make_shared<std::promise<void>>();
        auto fut = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_acks_.emplace_back(std::async(std::launch::async, [this, message, promise, timeout_ms]() {
                bus_->write_internal(channel_name_, message);
                if (timeout_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
                }
                promise->set_value();
            }));
        }
    }

private:
    std::mutex pending_mutex_;
    std::vector<std::future<void>> pending_acks_;
};
```

**验收**：`./star build` + InprocAckWriter 单元测试（写入后立即销毁 writer，无段错误）

---

### 3.4 T6：NativeMessageBus recorder_ 数据竞争

**问题**：`write_internal` 在锁外访问 `recorder_`

**修复方案**：将 recorder_ 访问纳入锁保护范围

```cpp
void write_internal(const std::string& channel_name, const MessageView& message) {
    std::vector<std::pair<uint64_t, Reader::Callback>> reader_callbacks;
    MessageRecorder* recorder = nullptr;
    bool should_record = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... 收集 reader_callbacks ...
        recorder = recorder_.get();  // 在锁内获取
        should_record = recorder && should_record_channel_(channel_name);
    }
    // 锁外执行回调
    for (const auto& [_, cb] : reader_callbacks) { cb(message); }
    // 录制也在锁外（recorder 是原始指针，对象生命周期由 enable_recorder 管理）
    if (should_record) {
        auto payload = message.encode_binary ? message.encode_binary() : std::vector<uint8_t>{};
        recorder->record(channel_name, type_name, now_ns, payload);
    }
}
```

**验收**：`./star build` + ThreadSanitizer 并发写入测试

---

### 3.5 T7：ShmMessageBus erase_reader 内存泄漏

**问题**：只删除 readers 不删除 reader_history

**修复方案**：

```cpp
void erase_reader(const std::string& channel_name, uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel_name);
    if (it == channels_.end()) { return; }
    it->second.readers.erase(id);
    it->second.reader_history.erase(id);  // 新增
}
```

**验收**：`./star build` + 反复创建/销毁 reader 的内存泄漏测试

---

### 3.6 T8：消息重复投递

**问题**：RoutedWriter 同时写 inproc 和 shm，RoutedReader 在两个 bus 上注册同一 callback

**根因分析**：

路由策略的核心矛盾：Writer 需要双写（inproc 给本地 reader，shm 给跨进程 reader），但 Reader 不应该双注册（否则本地 reader 收到两次）。

**正确语义**：
- 如果本进程是 producer：本地 reader 只从 inproc 读
- 如果本进程是 consumer（跨进程）：reader 只从 shm 读
- 纯本地 channel：只走 inproc

**修复方案**：

```cpp
auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> override {
    const bool is_producer = should_write_shm(descriptor.name);
    const bool is_consumer = should_read_shm(descriptor.name);

    std::shared_ptr<Reader> reader_inproc;
    std::shared_ptr<Reader> reader_shm;

    if (is_producer || (!is_producer && !is_consumer)) {
        // 本进程是生产者 或 纯本地 channel → 只从 inproc 读
        reader_inproc = inproc_->create_reader(descriptor, callback);
    }
    if (is_consumer && !is_producer) {
        // 本进程是跨进程消费者 → 只从 shm 读
        reader_shm = shm_->create_reader(descriptor, callback);
    }
    if (is_consumer && is_producer) {
        // 既是生产者又是消费者 → 本地 reader 从 inproc 读，跨进程 reader 从 shm 读
        // 但本地 reader 已经从 inproc 读到了，不需要再从 shm 读
        reader_inproc = inproc_->create_reader(descriptor, callback);
    }

    return std::make_shared<RoutedReader>(std::move(reader_inproc), std::move(reader_shm));
}
```

**RoutedWriter 也需要修正**：

```cpp
void write(const MessageView& message) override {
    // 总是写 inproc（本地 reader 从 inproc 读）
    if (inproc_) { inproc_->write(message); }
    // 仅当有跨进程消费者时才写 shm
    if (shm_ && should_write_shm(channel_name_)) { shm_->write(message); }
}
```

**验收**：`./star build` + 路由消息测试（同进程 reader 只收到一次消息）

---

### 3.7 T9：compute_percentile p=0.0 bug

**问题**：`ceil(0.0 * n) = 0`，`static_cast<size_t>(0) - 1` 对 size_t 下溢

**修复方案**：

```cpp
inline auto compute_percentile(const std::vector<uint64_t>& samples, double p) -> uint64_t {
    if (samples.empty()) { return 0; }
    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    if (p <= 0.0) { return sorted.front(); }
    if (p >= 1.0) { return sorted.back(); }
    const auto idx = static_cast<size_t>(p * sorted.size());
    return sorted[std::min(idx, sorted.size() - 1)];
}
```

**验收**：`./star test` + compute_percentile 边界测试（p=0, p=1, p=0.5, 空数组）

---

### 3.8 C1：TypedConstPtr type==nullptr 安全漏洞

**问题**：`type` 为 nullptr 时跳过类型检查，允许任意 `static_pointer_cast`

**修复方案**：

```cpp
template <typename _T>
auto as() const -> std::shared_ptr<const _T> {
    if (not ptr) { return nullptr; }
    if (not type) {
        // type 信息缺失时拒绝转换，而非跳过检查
        return nullptr;
    }
    if (*type != typeid(_T)) {
        return nullptr;
    }
    return std::static_pointer_cast<const _T>(ptr);
}
```

**验收**：`./star build` + TypedConstPtr 单元测试（type=nullptr 时 as() 返回 nullptr）

---

### 3.9 C2：LoggerManager 线程安全

**问题**：`set_logger`/`get_logger` 无同步，懒初始化可能双重创建

**修复方案**：

```cpp
class LoggerManager {
public:
    void set_logger(std::unique_ptr<Logger> logger) {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_ = std::move(logger);
    }

    auto get_logger() -> Logger* {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not logger_) {
            logger_ = std::make_unique<StdoutLogger>();
        }
        return logger_.get();
    }

private:
    std::mutex mutex_;
    std::unique_ptr<Logger> logger_;
};
```

**验收**：`./star build` + 多线程并发 get_logger/set_logger 测试

---

### 3.10 C3：RollingFileLogger 滚动失效

**问题**：`current_size_` 从不更新，滚动机制完全失效

**修复方案**：

```cpp
void log(Level level, const std::string& message, const char* file, int line, const char* function) override {
    if (level < level_) { return; }
    const auto formatted = format_message(level, message, file, line, function);
    const auto entry = "[" + level_string(level) + "] " + formatted + "\n";
    *current_ << entry;
    current_->flush();
    current_size_ += entry.size();  // 新增：更新当前文件大小
    if (should_roll()) { roll(); }
}
```

**验收**：`./star build` + RollingFileLogger 测试（写入超过 max_file_size 后确认滚动）

---

### 3.11 R1：observability channel_stats 编译错误

**问题**：`last_write_time_ns_` vs `last_publish_time_ns_` 命名不一致

**修复方案**：统一为 `last_write_time_ns_`（与 write/read 术语一致）

**验收**：`./star build` 编译通过

---

## 四、P1 修复方案（架构级修复）

### 4.1 T1+T12：数据表示三重冗余 + correlation_id 分裂

**这是整个重整的核心——根因 F 的直接修复。**

**当前问题**：
- MessageView 同时携带 `data`(TypedConstPtr) + `encode_binary`(function) + `encode_into`(function)
- ServiceRequest/Response 额外还有 `data`(SchemaValue)
- correlation_id 在 MessageView 中为 uint64_t，在 ServiceRequest 中为 CorrelationId

**修复方案：分离"载荷"与"序列化能力"**

```cpp
// 序列化能力（仅写入侧需要）
struct Serializable {
    std::function<size_t(uint8_t*, size_t)> encode_into;   // SHM loan 零拷贝
    std::function<std::vector<uint8_t>()> encode_binary;   // 录制/回退
    size_t size_hint{0};                                    // 预估大小，不触发序列化
};

// SHM 视图（跨进程读取侧）
struct ShmView {
    std::shared_ptr<void> mapping;
    const uint8_t* data{nullptr};
    size_t size{0};
    star::schema::SchemaTypeId schema_type_id;
    auto bytes() const -> std::span<const uint8_t>;
};

// 统一消息视图
struct MessageView {
    star::schema::SchemaTypeId schema_type_id;
    CorrelationId correlation_id;            // 统一为 CorrelationId

    star::base::TypedConstPtr payload;       // InprocPayload 或 ShmView
    Serializable serializable;               // 仅写入侧需要

    template <typename _T>
    auto as() const -> std::shared_ptr<const _T>;

    auto as_shm_view() const -> std::shared_ptr<const ShmView>;
    auto is_inproc() const -> bool;
    auto is_shm() const -> bool;
    auto has_serializable() const -> bool;
};

// 统一服务请求/响应
struct ServiceRequest {
    star::schema::SchemaTypeId schema_type_id;
    CorrelationId correlation_id;
    star::base::TypedConstPtr payload;       // 统一，删除 SchemaValue data
    Serializable serializable;
};

struct ServiceResponse {
    bool ok{true};
    std::string message;
    star::schema::SchemaTypeId schema_type_id;
    CorrelationId correlation_id;
    star::base::TypedConstPtr payload;       // 统一，删除 SchemaValue data
    Serializable serializable;
};
```

**关键变更**：
1. 删除 `MessageView::data` + `encode_binary` + `encode_into`，替换为 `payload` + `serializable`
2. 删除 `ServiceRequest/Response::data`(SchemaValue)，统一为 `payload`
3. `correlation_id` 统一为 `CorrelationId` 结构体
4. `estimate_payload_size` 使用 `serializable.size_hint`，不再调用 `encode_binary()`

**迁移策略**：
1. Phase 1：引入新字段，旧字段标记 `[[deprecated]]`
2. Phase 2：所有 Bus 实现切换到新字段
3. Phase 3：删除旧字段

**验收**：`./star build` + 全部现有测试通过 + 新增 MessageView/ServiceRequest 单元测试

---

### 4.2 S1：codegen 模板代码 4 倍重复

**问题**：`schema_read`/`schema_write`/`IsStdVector`/`IsStdMap`/`SchemaStructType` 约 90 行在每个 .s.h 中完整复制

**修复方案**：

1. 创建 `basement/schema/schema_codec_helpers.h`，包含所有公共模板
2. 修改 `schema_codegen.py`，生成文件改为 `#include "basement/schema/schema_codec_helpers.h"`
3. 修复头文件保护宏：改用 schema name 而非绝对路径

```python
# schema_codegen.py 修改
def guard_name(schema_name: str) -> str:
    text = schema_name.upper().replace(".", "_").replace("-", "_")
    return f"__{text}__"

# 生成文件头部改为：
# #include "basement/schema/schema_codec_helpers.h"
# 而非内联定义所有模板
```

**验收**：`./star build` + `./star test` + 检查 .s.h 文件不再包含重复模板代码

---

### 4.3 T10：FixedLayoutHeader 64KB 限制

**问题**：`total_size` 为 `uint16_t`，单条消息不能超过 64KB

**修复方案**：

```cpp
struct FixedLayoutHeader {
    uint32_t magic;
    uint32_t total_size;        // uint16_t → uint32_t，支持 4GB
    uint16_t fixed_section_size;
    uint16_t field_count;
};
```

同时添加越界检查：

```cpp
// FixedLayoutWriter::set_uint32 等
void set_uint32(size_t index, uint32_t value) {
    if (index >= fixed_slots_.size()) {
        throw RuntimeError("FixedLayoutWriter: index out of bounds");
    }
    fixed_slots_[index].u32 = value;
}
```

**验收**：`./star build` + FixedLayoutCodec 大消息测试（>64KB）

---

### 4.4 M1：头文件保护宏绝对路径

**问题**：codegen 使用 `output_path` 绝对路径生成 guard

**修复方案**：见 4.2，改用 schema name 生成 guard

**验收**：检查 .s.h 文件 guard 不包含 `/HOME/ZGY/` 等绝对路径

---

## 五、P2 修复方案（设计改进）

### 5.1 路由逻辑统一

**问题**：ChannelRegistry 和 ShmRoutedMessageBus 各维护一套 should_write_shm/should_read_shm

**修复方案**：ShmRoutedMessageBus 始终委托给 ChannelRegistry，消除本地 route_registry_ 和 shm_pub_channels_/shm_sub_channels_

**验收**：路由配置只在一处修改，两处行为一致

### 5.2 ACK/async 语义修正

**问题**：InprocAckWriter 不是真正的 ACK，NativeServiceBus::async_call_service 是伪异步

**修复方案**：
- InprocAckWriter → 重命名为 InprocSyncWriter（语义准确）
- async_call_service → 改为真正的异步（提交到线程池执行）

**验收**：语义与命名一致

### 5.3 Port 拆分为 EndpointDescriptor

**问题**：reader/writer/command 三种端点共用 Port 类型

**修复方案**：在 schema 中定义 ReaderEndpoint / WriterEndpoint / CommandEndpoint，各自只包含相关字段

**验收**：reader 不再携带 ack/ack_type/status/status_type 字段

### 5.4 encode_fixed_layout / encode_fixed_layout_into 去重

**问题**：codegen 生成的两个方法逻辑 100% 重复

**修复方案**：提取私有 `build_fixed_layout(FixedLayoutWriter&)` 方法

**验收**：.s.h 文件中每个 struct 的序列化逻辑只出现一次

### 5.5 命名规范统一

**问题**：操作类函数使用 snake_case，View 类 Has_ 使用 PascalCase

**修复方案**：
- 虚接口方法（declare_channel/create_writer 等）暂保持 snake_case（改动影响面大，需 spec 同步）
- View 类 Has_ → has_（codegen 修改）
- core 层操作函数：reset → Reset，advance → Advance

**验收**：命名规范检查脚本通过

### 5.6 Logger 基类统一级别过滤和格式化

**问题**：三个子类各自实现 `if (level < level_) return;` 和 `format_message()`

**修复方案**：

```cpp
class Logger {
public:
    void log(Level level, const std::string& message, const char* file, int line, const char* function) {
        if (level < level_) { return; }
        const auto formatted = format_message(level, message, file, line, function);
        write(level, formatted);  // 子类只实现 write
    }

protected:
    virtual void write(Level level, const std::string& formatted) = 0;
};
```

**验收**：子类不再重复级别过滤和格式化逻辑

---

## 六、执行顺序与依赖关系

```
阶段 0：P0 快速修复 ──────────────────────────── 立即执行
    │ 0.1 T9  compute_percentile bug 修复
    │ 0.2 C1  TypedConstPtr 安全漏洞
    │ 0.3 C2  LoggerManager 线程安全
    │ 0.4 C3  RollingFileLogger 滚动失效
    │ 0.5 R1  observability channel_stats 编译错误
    │ 0.6 T7  erase_reader 内存泄漏
    │ 0.7 T6  recorder_ 数据竞争
    │
    ▼
阶段 1：SHM 跨进程修复 ──────────────────────── 依赖阶段 0
    │ 1.1 T3+T4  ShmChannel 跨进程互斥 + 初始化竞态
    │ 1.2 T5     InprocAckWriter detached 线程
    │ 1.3 T8     消息重复投递
    │ 1.4 T2     ShmServiceBus 不可用
    │
    ▼
阶段 2：数据表示重设计 ──────────────────────── 依赖阶段 1
    │ 2.1 T1+T12  MessageView/ServiceRequest 重设计
    │ 2.2 T10     FixedLayoutHeader 64KB 限制
    │
    ▼
阶段 3：Schema/Manifest 修复 ─────────────────── 依赖阶段 2
    │ 3.1 S1  codegen 模板重复提取
    │ 3.2 M1  头文件保护宏修复
    │ 3.3     encode_fixed_layout 去重
    │ 3.4     Port 拆分为 EndpointDescriptor
    │
    ▼
阶段 4：设计改进 ────────────────────────────── 依赖阶段 3
    │ 4.1 路由逻辑统一
    │ 4.2 ACK/async 语义修正
    │ 4.3 Logger 基类统一
    │ 4.4 命名规范统一
    │ 4.5 S2  schema_bootstrap.h 反向依赖
    │ 4.6 S3+S4  SchemaRegistry 并发安全
    │
    ▼
验收：./star build + ./star test + ./star check ┘
```

---

## 七、成熟度评估

| 维度 | 当前 | P0后 | P1后 | P2后 | 全部后 |
|------|------|------|------|------|--------|
| 架构一致性 | 70% | 75% | 88% | 92% | 95% |
| 概念完整性 | 70% | 75% | 85% | 90% | 95% |
| 代码可维护性 | 65% | 70% | 82% | 88% | 92% |
| 测试覆盖 | 45% | 50% | 60% | 65% | 70% |
| **综合** | **65%** | **70%** | **82%** | **87%** | **90%** |
