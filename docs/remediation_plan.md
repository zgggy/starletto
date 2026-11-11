# Starletto 底层修复方案

> 依据：2026-11-11 对 `basement/` 全部 11k 行的复审。
> 原则：只列**已用代码或实验证实**的问题，每条给出证据、修法、验收标准。
> 分级：P0 = 会静默产出错数据或让系统永久停摆；P1 = 关键能力缺失或静默失效；P2 = 演进与可维护性。

## 状态速览

| 级别 | 数量 | 主题 |
|---|---|---|
| P0 | 6 | 零拷贝正确性、崩溃恢复、静默停投、越界、容量硬限 |
| P1 | 5 | 跨进程 RPC、路由/录制/降级静默、资源泄漏 |
| P2 | 6 | 类型身份、schema 演进、codegen 扩展性、观测、线程模型 |

---

## P0 · 正确性与可用性

### P0-1 seqlock 撕裂窗口（最严重，且是上一轮修复引入的）

**问题**：`ShmSlotView::stable()` 判定"seq 推进不足 slot_count 即本槽未变"。但写入顺序是 `memcpy → sizes → seq`，`commit()` 才推进 seq。写方 `loan()` 拿到槽位开始 memcpy 时，全局 seq 仍是 `observed_seq + 3` → `stable()` 返回 **true**，而数据正在被覆写。

**证据**（确定性实验，非时序）：

```
seq=4 时 stable=1（正确：本槽尚未被写）
memcpy 进行中（seq 仍为 4）stable=1  ← 应为 0
此时 bytes()[0]=0xBB（已被覆写，但视图仍自称稳定）
```

且 `stable()` 是**访问前**检查，天然 TOCTOU；`peek_view()` 完全不检查（`component.h:137-158`）。

**修法**：
1. `ShmHeader` 增加 per-slot 代际 `std::atomic<uint32_t> gen[kShmMaxSlots]`：
   - 写方 `loan()` 后、memcpy **前**：`gen[slot] += 1`（变奇数 = 写入中）
   - `commit()` 末尾：`gen[slot] += 1`（变偶数 = 稳定）
   - `ShmSlotView` 记录 `observed_gen`
2. `stable()` 改为：`gen[slot]` 仍等于 `observed_gen` **且为偶数**
3. 读侧协议改为「解码前检查 → 解码 → **解码后复核**」，不一致则丢弃并计入 `rejected`
4. `peek_view()` 也必须走同一复核
5. `kShmVersion` 3 → 4，attach 时校验版本

**验收**：把 P0-1 的实验固化为回归测试（现有 `shm_channel_test` 增加"memcpy 中 stable 必须为 false"）；`shm_stress_test` 增加"解码后复核"路径覆盖；`selftest` 与 112 用例全绿。

**代价**：shm 头布局变化（需同步升级所有进程；当前无跨版本需求，可接受）。

### P0-2 写锁无 owner-death → 通道永久毒化

**问题**：`write_lock` 是共享内存里的裸 `atomic<uint32_t>`，非 `FUTEX_ROBUST`，无 owner、无超时。写方在 `loan()` 与 `commit()` 之间崩溃 → 锁永为 1 → 该通道所有写者永久阻塞；段名由通道名哈希决定且跨运行复用 → **下次启动继承毒状态**。

**证据**：`shm_channel.h:30-44`（无限 futex 等待）、`:212`（loan 加锁）、`:186-196`（只有 commit 解锁）；全仓无 `FUTEX_ROBUST`。

**修法**（两层）：
1. 段名加启动 epoch：`/starletto_<hash>_<boot_id|start_ts>`，让崩溃遗留段不再被新进程复用（简单、立即消除"跨启动毒化"）
2. `write_lock` 升级为「owner pid + 单调时间戳」的自旋锁：
   - 加锁时写入 `owner_pid` 与 `lock_time_ns`
   - 等待超过阈值（如 200ms）且 `owner_pid` 已不存在（`kill(pid,0)` 失败）→ 强制置 0 并记 WARN
3. 或采用 `PTHREAD_PROCESS_SHARED` robust mutex（更标准，但要求共享内存内放 pthread 对象）

**验收**：新增测试——子进程持锁后 `SIGKILL`，父进程能在阈值内恢复写入；连续 100 次崩溃/恢复循环无线程泄漏。

### P0-3 订阅回调抛异常 → 进程 terminate

**问题**：`poll_loop()` 与 `deliver_to_reader()` 里 `cb(msg)` 裸调，无 try。任何组件回调抛异常 → 异常穿透线程 → `std::terminate`。

**证据**：`shm_message_bus.h:98-119`（poll_loop 无 try）、`:410`（`cb(msg)` 无 try）。

**修法**：`deliver_to_reader` 内用 try/catch 包裹 `cb(msg)`，捕获后 `logE` + 计入该 reader 的失败计数（新增 `reader_failures` 指标），不中断轮询。

**验收**：新增测试——注册一个必抛异常的回调，断言进程存活、失败计数递增、后续消息仍能投递。

### P0-4 单帧 >1MiB → 永久降级

**问题**：槽位 4×1MiB 写死（`resolve_capacity/resolve_slot_size` 忽略参数）；超限返回 `ENCODE_FAILED`，而 `shm_routed_message_bus.h:220-223` 一旦异常就 `shm_.reset()` **永久**降级该通道。且本进程订阅者已收到消息、远端从此收不到（部分投递）。

**证据**：`shm_channel.h:20-27`、`:334-339`；`shm_message_bus.h:321-326`；`shm_routed_message_bus.h:220-223`。

**修法**：
1. `ChannelOptions` 增加 `slot_size` / `slot_count`，由 conf 的 `qos_profile` 透传；默认值保持现状
2. 超限不再永久降级：区分「本次编码失败」与「通道不可用」；连续 N 次失败才降级，且降级可恢复（定时重试）
3. 部分投递语义要显式：要么先写 shm 再投 inproc，要么在 `WriteResult` 里标明 `partial=true`

**验收**：新增测试——配置 2MiB 槽位的通道，写入 1.5MB 消息成功；写入超限消息只失败本次，后续正常。

### P0-5 `DROP_NEWEST` 永久停投

**问题**：`qos_manager.h` 的 `DROP_NEWEST` 分支在 `history` 非空时永远返回 `should_deliver=false`，而 history 没有任何消费者（只有 reader 析构时清理）→ 填满后永久丢全部消息。

**证据**：`qos_manager.h:30-33`（depth==1）、`:44-47`（depth>1）。

**修法**：明确 history 的语义是「为 `peek_latest` 保留最近 N 条」而非队列；`DROP_NEWEST` 的判定应基于时间窗口或「上次投递时间」，而不是 history 是否非空。最简修法：`DROP_NEWEST` 下不阻塞投递，只更新 history（与 `KEEP_LAST` 的差异体现在 history 更新策略上），或直接禁用该策略并报配置错误。

**验收**：新增测试——`DROP_NEWEST` 通道连续写 100 条，断言各 reader 收到至少 1 条且不出现"第 2 条起全丢"。

### P0-6 `ensure_remaining` 整数溢出 → 越界读

**问题**：`pos + need > size()` 在 `need` 接近 `SIZE_MAX` 时回绕，检查通过后 `subspan(pos, huge)` 触发 UB。损坏/恶意 payload 可直接崩溃进程。

**证据**：`binary_codec.h:467-472`。

**修法**：改为 `need > size() - pos`（先减后比，不会回绕），并给所有 `read_varint` 调用点加同样的保护；补 depth 与 size 上限。

**验收**：新增 fuzz 式测试——构造长度字段为 `0xFFFFFFFFFFFFFFFF` 的 payload，断言抛 `RuntimeError` 而不是崩溃。

---

## P1 · 能力缺失与静默失效

### P1-1 跨进程 RPC 实际不存在

**问题**：`RuntimeFactory` 无条件创建 `NativeServiceBus`，`ShmServiceBus` 只在 selftest 里手工构造。多进程部署下 `call()` 直接抛 unavailable。

**证据**：`runtime_factory.h:90`；`shm_service_bus.h` 无生产调用点。

**修法**：按 transport 名称选择 service bus 实现；同时把服务通道纳入 route plan（当前 `build_route_plan` 只看 inputs/outputs，服务 topic 永远 `INPROC_ONLY`）。

### P1-2 routed 模式录制完全失效

**问题**：`ShmRoutedMessageBus` 未继承 `RecordableBus`，`recordable_bus()` 返回 null，`record: true` 静默无效。

**证据**：`shm_routed_message_bus.h:19`；`runtime_factory.h:46-48`。

**修法**：`ShmRoutedMessageBus` 委托给内部 `inproc_`（它已经是 `RecordableBus`），并在 routed 写路径的 shm 分支同样触发录制。

### P1-3 路由漏配/降级全程静默

**问题**：route plan 漏项、group 拼错、通道降级，全部表现为"写入返回 OK，对端收不到"。

**修法**：
1. `WriteResult` 增加 `peer_expected` / `degraded` 显式字段，`ok()` 不掩盖降级
2. 启动时校验：route plan 中每条通道的 producer/consumer 都存在
3. `monitor` 增加"跨进程通道健康"视图（peer 存活、末尾 seq 差）

### P1-4 shm 段泄漏与 attach 不校验

**问题**：子进程 `_exit()` 跳过析构 → ref_count 不减 → 段泄漏；attach 只查 `st_size`，不校验 magic/version，且会回写别人的共享头。

**证据**：`app.cc:697`；`shm_channel.h:101,150-158`。

**修法**：attach 校验 magic/version/布局一致性，不一致直接失败并提示清理；段名加 epoch（与 P0-2 同一改动解决）；`_exit` 前显式 close 或在 supervisor 侧清理。

### P1-5 `peek_view` 无稳定性校验

**问题**：`peek_view` 直接读 `slot->data`，不检查 `stable()`，是 P0-1 的另一条暴露面。

**修法**：随 P0-1 一并修复（解码前后各复核一次）。

---

## P2 · 演进与可维护性

| # | 问题 | 修法方向 |
|---|---|---|
| P2-1 | 类型身份靠 `typeid`/mangled name，跨 DSO/插件卸载不成立 | `TypedConstPtr` 改携带 `SchemaTypeId`（name+version+hash）作为唯一身份；`typeid` 仅用于 debug 断言 |
| P2-2 | schema 演进机制缺失：无 reserved/deprecated、hash 手写、registry 单版本 | 加 `reserved` 关键字与 lint；`schema_hash` 由 codegen 计算写入；registry 支持同 name 多版本并存 |
| P2-3 | 每个 reader 一个轮询线程（8 相机 × 3 消费者 = 24 线程） | 改为每通道一个 epoll/事件线程，或共享一个轮询池 |
| P2-4 | codegen 单文件单 root、跨文件无引用、helper 可能 ODR | 拆 IR + per-language emitter；先支持跨文件 import 与多 root；helper 加文件级前缀 |
| P2-5 | 日志无 pid、同步写、无结构化 | 加 pid/异步队列/JSON；per-module 级别接线 |
| P2-6 | `IClock` 有抽象零引用，仿真时钟无法驱动 futex | 要么接入 `RuntimeContext` 并贯穿调度/futex/录放，要么删除 |

---

## 实施顺序与验收

```mermaid
flowchart LR
    P0["阶段一<br/>P0-1 ~ P0-6<br/>正确性"] --> P1["阶段二<br/>P1-1 ~ P1-5<br/>能力与静默"]
    P1 --> P2["阶段三<br/>P2-1 ~ P2-6<br/>演进"]
```

| 阶段 | 内容 | 门槛 |
|---|---|---|
| 一 | P0 全修 | `star check` 10/10；新增 6 组回归测试；`selftest` 含崩溃恢复场景 |
| 二 | P1 全修 | 多进程 RPC 端到端可跑；routed 录制可回放；`monitor` 显示跨进程健康 |
| 三 | P2 逐项 | 每项独立提交；P2-1/P2-2 需先出行 wire format 规范文档与 golden vectors |

**不建议跳阶段**：P1/P2 的改动都建立在 P0 的 shm 头变更之上（`kShmVersion` 升级），先做 P0 可以避免二次改格式。

> 说明：P0-1 是上一轮修复引入的缺陷，应最先修，并在提交信息里注明「修正此前的 seqlock 判定逻辑」。
