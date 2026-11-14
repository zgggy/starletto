# Starletto 底层修复方案

> 依据：2026-11-11 对 `basement/` 全部 11k 行的复审。
> 原则：只列**已用代码或实验证实**的问题，每条给出证据、修法、验收标准。
> 分级：P0 = 会静默产出错数据或让系统永久停摆；P1 = 关键能力缺失或静默失效；P2 = 演进与可维护性。

## 状态速览

| 级别 | 数量 | 主题 |
|---|---|---|
| P0 | 6 | **全部已修**：seqlock、写锁 owner-death、回调异常、单帧降级、DROP_NEWEST、解码溢出 |
| P1 | 5 | **全部已修**：跨进程 RPC、routed 录制、路由静默、段校验、peek_view |
| P2 | 6 | 类型身份、schema 演进、codegen 扩展性、观测、线程模型 |

---

## P0 · 正确性与可用性

### P0-1 seqlock 撕裂窗口 ✅ 已修（2026-11-11）

**问题**：`ShmSlotView::stable()` 判定"seq 推进不足 slot_count 即本槽未变"。但写入顺序是 `memcpy → sizes → seq`，`commit()` 才推进 seq。写方 `loan()` 拿到槽位开始 memcpy 时，全局 seq 仍是 `observed_seq + 3` → `stable()` 返回 **true**，而数据正在被覆写。且检查在访问前，天然 TOCTOU；`peek_view()` 完全不检查。

**修复内容**：

| 改动 | 位置 |
|---|---|
| `ShmHeader` 增加 per-slot 代际 `std::atomic<uint32_t> gen[kShmMaxSlots]`，`kShmVersion` 3 → 4 | `shm_channel.h` |
| 写方 `loan()` 后置奇（写入中），`commit()` 中先置偶再推进 seq | `shm_channel.h` |
| `Loan` 未提交即析构时置偶（异常路径不会让通道永久卡"写入中"）；同时禁止拷贝（避免双重解锁） | `shm_channel.h` |
| `read()` 双检：读 size 前 gen 必须为偶，读后 gen 必须未变 | `shm_channel.h` |
| `ShmSlotView` 改携带 `gen_addr` / `observed_gen`，`stable()` = 代际相同且为偶 | `shm_views.h` |
| 消费侧**解码后再复核**：`decode_message_shared`、`peek_view`、`decode_service_envelope` | `component.h`、`shm_service_bus.h` |
| 段名带版本 `/star_v%02u_<hash>`；attach 校验 magic/version/header 大小/布局，且不再回写别人的 header | `shm_message_bus.h`、`shm_channel.h` |

**验收证据**（同一段实验代码，修复前后对比）：

```
修复前：memcpy 进行中（seq 仍为 4）stable=1   ← 撕裂窗口
修复后：memcpy 进行中（seq 未推进）stable=0   ← 已拒绝
        bytes() 为空 = 1（拒绝返回撕裂数据）
        loan 未提交即释放后 stable=0（代际已变）
```

新增回归用例 `ShmSlotView reports unstable while a writer holds a loan on its slot`，用 `loan()` 精确复现写入窗口，不依赖时序。`star selftest`（含多进程 shm）、17 套 113 用例、`star check` 10/10 全绿。

**遗留**：本次未改 `ShmChannel::read()` 的 `last_seq` 推进策略——遇到"写入中"返回 nullopt 不推进，由调用方重试；已在高频写压力测试中验证不会饿死。

### P0-2 写锁无 owner-death → 通道永久毒化 ✅ 已修（2026-11-11）

**问题**：`write_lock` 是共享内存里的裸 `atomic<uint32_t>`，非 robust、无 owner、无限等待。写方在 `loan()` 与 `commit()` 之间崩溃 → 锁永为 1 → 该通道所有写者永久阻塞，且无日志、无自愈。

**修复内容**：

| 改动 | 说明 |
|---|---|
| `ShmHeader` 增加 `write_lock_owner_pid` / `write_lock_owner_ns` / `lock_recovery` / `lock_recoveries` / `gen_parity_dirty` | 记录持锁者与恢复状态 |
| `spin_lock` 改为有界等待（每 50ms 复核），超过 200ms 且确认 owner 已死则复位 | 从"无限等待"变为"可恢复" |
| 恢复判定用 `kill(pid, 0)`：仅 `ESRCH` 视为死亡，`EPERM` 视为存活 | 不会误杀存活进程 |
| 复位操作由 `lock_recovery` 守卫串行化，且复位前复核 owner 未变 | 避免两个等待者互相踩掉刚获得的锁 |
| **代际奇偶归一化**：崩溃写方会把 `gen` 留在奇数位，恢复时置 `gen_parity_dirty`，下一个持锁者把奇数代际 +1 归偶 | 否则后续写入的 +1 会让它变偶，读方会把已损坏的槽当成稳定 |
| 段名带版本 `/star_v%02u_<hash>`，attach 校验 magic/version/header 大小/布局 | 崩溃遗留的旧布局段不再被复用 |

**验收证据**：新增用例 `ShmChannel recovers the write lock from a dead owner`——子进程持锁后 `_exit(0)`（跳过析构），父进程 `waitpid` 回收后写入，断言：恢复计数 = 1、耗时 < 5s、之后能正常读到数据。**双向验证**：临时禁用恢复逻辑后该用例直接卡死（`timeout` 退出码 124），恢复后 9/9 通过。

**运行前提**：恢复依赖 `kill(pid,0)` 能返回 `ESRCH`，因此**死掉的 worker 必须被回收**（supervisor 的 `wait_all` 已如此）。僵尸进程会被视为存活而阻塞恢复。

**遗留**：pid 复用窗口（崩溃进程的 pid 被新进程复用）会导致误判为存活；概率低且有 WARN 日志，暂不处理。

### P0-3 订阅回调抛异常 → 进程 terminate ✅ 已修（2026-11-11）

**问题**：`poll_loop()` 与 `deliver_to_reader()` 里 `cb(msg)` 裸调，无 try。任何组件回调抛异常 → 穿透线程 → `std::terminate`。

**修复**：`ShmMessageBus` 与 `NativeMessageBus` 的回调派发全部加 try/catch；捕获后 `logE` 并累加 `ChannelStats::callback_failures`（已进入 snapshot 与 `format_stats_line`，`star monitor` 可见）。

**验收**：新增 2 个用例——native 侧「坏回调 + 好回调」同时注册，断言进程存活、好回调仍收到全部消息、`callback_failures == 2`；shm 侧在轮询线程上抛异常，断言进程存活且其他 reader 正常收消息。

### P0-4 单帧 >1MiB → 永久降级 ✅ 已修（2026-11-11）

**问题**：超限返回 `ENCODE_FAILED`（或经 `channel->write` 抛异常变成 `EXCEPTION`），而 routed 写路径对这两种状态一律 `degrade_channel()` + `shm_.reset()` → **通道永久**失去跨进程能力，后续正常帧也发不出去。

**修复**：
- 区分「单帧编码失败」与「通道故障」：`ShmMessageBus` 在编码后显式比对 `bytes.size() > slot_size()`，判为 `ENCODE_FAILED` 并附明确错误信息；routed 写路径只在 `EXCEPTION` 时降级
- 不再 `shm_.reset()` 销毁 writer；降级改为**带 TTL（5s）**，过期自动重试，支持对端重启后自愈
- 部分投递语义可判别：`WriteResult.inproc_ok=true` + 非 OK 状态即表示「本进程订阅者已收到、远端没收到」

**验收**：新增用例 `an oversized message does not permanently degrade the channel`——发 1MiB+1 帧断言 `ENCODE_FAILED`，随后 10 帧正常消息全部成功且被收到。双向验证：恢复旧逻辑后该用例失败（后续帧 SHM_DISABLED）。

**遗留**：单槽仍写死 1 MiB（`resolve_capacity/resolve_slot_size` 忽略参数），逐通道可配置需要贯通 `ChannelOptions` → conf → plan，留待 P1 阶段。当前应对大帧（原始相机/激光雷达）仍需应用层切块。

### P0-5 `DROP_NEWEST` 永久停投 ✅ 已修（2026-11-11）

**问题**：`DROP_NEWEST` 分支在 `history` 非空时永远返回 `should_deliver=false`，而 history 没有任何消费者 → 填满后永久丢弃全部消息。

**修复**：该策略需要「消费者已消费」的信号才能实现，而当前通道没有背压/消费通知，语义上无法正确实现。因此：
- `declare_channel` 明确拒绝 `DROP_NEWEST`，报错提示改用 `KEEP_LAST`（把静默全丢换成启动期可见错误）
- `apply_qos` 的 `DROP_NEWEST` 分支删除，退化为 `KEEP_LAST`——万一绕过校验也绝不静默停投

**验收**：新增用例 `declare_channel rejects DROP_NEWEST as unimplemented`，断言声明时抛 `RuntimeError`。

### P0-6 `ensure_remaining` 整数溢出 → 越界读 ✅ 已修（2026-11-11）

**问题**：`pos + need > size()` 在 `need` 接近 `SIZE_MAX` 时回绕，检查通过后 `subspan(pos, huge)` 触发 UB。

**修复**：改为 `need > size() - pos`（先减后比）。

**验收**：新增用例 `decode rejects a length prefix that would overflow the buffer`，构造 tag + 10 字节超长 varint，断言抛 `RuntimeError`。
**须如实说明**：该用例**无法区分修复前后**——旧实现因 libc++ 的 `subspan` 会把 `count` 截断到 0、随后 `cur->pos` 回绕并在下一轮边界检查抛同一异常而「碰巧安全」。修复消除的是那条路径上的未定义行为（越界指针运算 + pos 回绕），并非可观测结果。

---

## P1 · 能力缺失与静默失效

### P1-1 跨进程 RPC 实际不存在 ✅ 已修（2026-11-11）

**问题**：`RuntimeFactory` 无条件创建 `NativeServiceBus`；且服务通道从不进入路由计划。多进程下客户端 `call()` 直接抛 unavailable。

**修复（三处联动）**：
1. `RuntimeFactory`：`shm_routed` 下创建 `ShmServiceBus`（依赖 routed 总线的通道与路由），其余情况保持 `NativeServiceBus`
2. `build_process_plan`：把 `plan.services` 折算成 `<name>/request` 与 `<name>/response` 两条通道（server 消费 request / 生产 response，client 反之），使它们进入路由计划；`plan_builder` 把 `commands` 块标记为 `role=server`
3. `parameter_component` 改为**声明式**：服务由 `component.conf` 的 `commands` 块声明、`auto_configure` 创建、`handle_service_request` 按端口名分发；conf 未声明时（工具侧直接实例化组件）退回内置注册，保持 `star param` 可用

**验证**：新增 3 个用例——`build_process_plan` 折算出两条通道、`shm_routed` 下工厂返回 `ShmServiceBus`、默认传输仍是 `NativeServiceBus`。多进程 `star run --multi-process setting` 两 worker 正常退出；`star param list/get` 正常。

**遗留（已关闭，2026-11-12）**：新增 `transport_rpc_multiprocess` 用例——fork 出真正独立的两进程，A 进程 RpcServer、B 进程 RpcClient，经 shm 路由完成 request/response；已双向验证（删掉 request 通道路由后调用超时失败）。

### P1-2 routed 模式录制完全失效 ✅ 已修（2026-11-11）

**问题**：`ShmRoutedMessageBus` 未继承 `RecordableBus`，`recordable_bus()` 返回 null，`record: true` 静默无效。

**修复**：继承 `RecordableBus`，把 4 个接口委托给内部 `NativeMessageBus`——routed 写路径本来就先走 inproc，录制自然覆盖。

**验证**：新增用例——routed 模式开启录制、写 3 条，回放读出 3 条。

### P1-3 路由漏配 / 降级全程静默 ✅ 已修（2026-11-11）

**问题**：route plan 漏项时通道静默退化为 inproc-only，写入仍返回 OK；降级也无任何日志。

**修复**：
- 新增 `ChannelRegistry::has_route_plan()` / `is_in_route_plan()`
- `ShmRoutedMessageBus` 在声明通道时检查：若存在路由计划但缺本通道，**每通道 WARN 一次**，明确提示「跨进程订阅者收不到」
- `degrade_channel` 增加 WARN（含 TTL 时长），降级与恢复不再无声

**验证**：新增用例——计划中缺失的通道仍能在进程内送达，但 `shm_required=false`（可判别为未路由）；WARN 日志已实测输出。

### P1-4 shm 段泄漏与 attach 不校验 ✅ 主体已修（随 P0-1/P0-2）

- attach 现在校验 magic / version / header 大小 / 布局，且不再回写别人的共享头
- 段名带版本号，旧布局段不再被误 attach
- **已根治（2026-11-14）**：header v5 增加持有者 pid 表（最多 32 个/段），attach 与析构时用 `kill(pid,0)` 清扫已死进程的槽位；清扫后无存活持有者即判定为孤儿段并原地重建（`holder_recoveries` 计数可观测）。不再依赖 ref_count。回归：`orphan shm segment is reclaimed...`、`shm segment with a live holder is never treated as orphan`，均已双向验证。

### P1-5 `peek_view` 无稳定性校验 ✅ 已随 P0-1 修复

## P2 · 演进与可维护性

| # | 问题 | 修法方向 |
|---|---|---|
| P2-1 ✅ | 类型身份靠 `typeid`/mangled name，跨 DSO/插件卸载不成立 | `TypedConstPtr` 改携带 `SchemaTypeId`（name+version+hash）作为唯一身份；`typeid` 仅用于 debug 断言。**已落地（2026-11-12）**：`basement/core/type_id.h`、写入路径盖章、通道首写采纳 |
| P2-2 ✅ | schema 演进机制缺失：无 reserved/deprecated、hash 手写、registry 单版本 | **已完成（2026-11-12）**：registry key=name+version、`lookup` 区分 hash 未知/不一致、FNV-1a 内容哈希、lint 校验 hash/未定义类型/map key/version；逐通道 `decode_policy` 接通 conf→plan→通道→解码；`def/root` 块支持 `reserved 2, 4;`（与字段 id 冲突/重复保留 id 报错，reserved 集合参与内容哈希，fmt 保留）；`star check` 新增 codegen 自检（reserved/多 root/哈希稳定/import 循环与缺失/未定义类型） |
| P2-3 ✅ | 每个 reader 一个轮询线程（8 相机 × 3 消费者 = 24 线程） | 改为**每通道一个轮询线程**，同通道的多个 reader 共享读取结果、各自按 QoS 分发。**已落地（2026-11-12）**：`ShmReader` 变成纯注册句柄，`ChannelPoller` 由首个 reader 启动、最后一个退出时回收；reader 析构停线程并 join（锁外）保证在途回调结束。回归：同通道 3 reader → `poller_count()==1` 且各收 3 帧；双通道 → 2 线程；全部退出 → 0 |
| P2-4 ✅ | codegen 单文件单 root、跨文件无引用、helper 可能 ODR | **已完成（2026-11-12）**：拆为 `codegen/{ir,parser,loader,cpp,text}.py`，`schema_codegen.py` 只留 CLI；多 root（独立身份 `<schema>.<root>`、哈希按可达闭包）；`import "path"` 跨文件引用（直接可见、符号冲突报错、`--include-root` 决定 include 路径、descriptor children 跨文件、循环检测）；helper 名加 schema 前缀消除 ODR；meson 依赖 codegen 源码。回归：`schema_multi_root` 2 用例 + `schema_import` 2 用例 |
| P2-5 ✅ | 日志无 pid、同步写、无结构化 | 加 pid/异步队列/JSON；per-module 级别接线。**已落地（2026-11-12）**：文本行带 `pid=`/`tid=`；`LogRecord` + `LogFormat::{TEXT,JSON}`；`AsyncLogger` 有界队列（满则丢弃计数，flush/析构排空）；`module.conf` 新增 `logging.level`，经 plan 到 host 设置 per-module 级别，调度线程进入任务时切换 `ModuleLogScope`（查表一次，不进热路径）；`STAR_LOG_FORMAT/ASYNC/LEVEL` 环境变量。回归：`core_logger` 6 用例 + plan 解析/透传用例 |
| P2-6 ✅ | `IClock` 有抽象零引用，仿真时钟无法驱动 futex | **已删除（2026-11-12）**：确认零 include、零实例化；接入 `now()` 也无法驱动 futex/timer/回放节奏，真正需要的是「统一可注入时钟 + 可显式唤醒的等待原语 + 回放驱动器」三件套（见 spec/basement_core §8.4）。删除后文档不再误报「仿真已就绪」 |
| P2-7 ✅ | **本轮复审发现（2026-11-12）**：shm 写路径优先固定布局，读路径固定调用 `from_binary`——生成消息跨进程必然解析失败（固定布局帧以 `0x5354` 开头，TLV 解析读到非法 wire type）。此前没有双进程 + 生成消息的用例，所以一直未暴露 | shm 默认写二进制 TLV（可演进格式）；固定布局帧头升级 v2 并携带 `schema_version`+`schema_hash`，读取方按帧头自动选解码器并校验身份；固定布局明确限定同版本使用。回归：`transport_shm_payload_encoding` 2 用例 + `schema_fixed_layout` 4 用例 + `Component decode_payload selects the decoder from the frame header` |
| P2-8 ✅ | 录制格式 v1 只有 C++ 类型名，没有 schema 身份；时间戳只有单调时钟，脱离录制机器无法还原墙钟，跨版本回放不可诊断 | 录制格式 v2：头部墙钟/单调双锚点；条目携带 schema name+version+hash；回放前 `validate_against()` 校验本地编译身份；v1 文件仍可读。回归：`record_replay` 9 用例（新增身份/摘要/墙钟/校验/v1 兼容 5 个） |
| P2-9 ✅ | 服务与通道双轨：`ServiceBus` 接口 + 两套实现 + 工厂动态选择；服务流量不进统计/录制，类型校验缺失，跨进程错误在 shm 实现里被丢弃（handler 异常 → 客户端 5 秒超时） | 撤销服务抽象，服务并入普通通道（决策四）：`RpcClient`/`RpcServer` 只依赖 `MessageBus`；信封携带 `ok/message`；删除三个 service_bus 头文件。回归：`transport_service_concurrency` 6 用例 + `runtime_plan` 服务可调用 + selftest 200 次跨总线 soak |

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

---

## 附：2026-11-14 底层复审（已修）

| # | 问题 | 证据 | 修复 |
|---|---|---|---|
| A1 | 空载荷被 shm 写路径判为编码失败 | `shm_message_bus.h` 对 `bytes.empty()` 直接返回 `ENCODE_FAILED`；而全 optional 消息的合法编码就是空字节（读侧已有缺失策略） | 允许 size=0 写入；`shm accepts an empty payload as a valid message`，已双向验证 |
| A2 | `NativeMessageBus` reader 析构不等在途回调 | 写方拷贝回调后释放锁再执行；组件卸载（reader 析构）时回调可能仍在跑，捕获的 `this` 已销毁 | `ReaderSlot{in_flight, cv}`；析构等 in_flight 归零。递减与 predicate 同锁（避免丢唤醒）；`destroying a reader waits for its in-flight callback` |
| A3 | 多进程服务通道无法路由 | `build_process_plan` 只知道 server 组；客户端组不在 component.conf 里，`run_multi_process` 推不出 `request` 的生产者，跨进程调用退化为 inproc | 服务通道登记为系统读写通道（始终 shm），supervisor 用 `--system-channels` 广播给所有 worker；`rpc works across processes through system service channels`，已双向验证 |
| A4 | `replay speed<=0` 除法无防护 | `pace_to` 直接 `elapsed / speed`，0 会产生 inf/UB | 非正值退化为最快回放 |
| A5 | selftest 用「空编码」当编码失败 | A1 修复后该场景合法，selftest 误报 | 改用 8MB 超槽位帧（真正的单帧过大） |
| A6 | shm 段名用 `std::hash<std::string>`，实现定义 | libc++ 与 libstdc++ 算法不同 → 跨编译器构建的进程算出不同段名，各自 attach、写入/读取都「成功」但收不到对方数据 | 新增 `core/stable_hash.h`（FNV-1a），段名改用稳定哈希，`kShmVersion` 升到 6；golden 值单测锁定算法 |

---

## 后续整改 P0.1：实时 wire 身份（已完成 2026-11-14）

- `kShmVersion` → 7；每个槽 payload 前增加逐帧帧头：
  `magic|encoding|name_len|schema_version|schema_hash|payload_size|schema_name|payload`
- 读侧 `MessageView.schema_type_id` 改为**发送方身份**（此前填本地 descriptor）
- `Component::decode_payload` 校验发送方身份：name 不一致直接拒绝；version 不同
  交给 `decode_policy`；同版本 hash 不同按 `type_mismatch`（默认 error）
- 固定布局 payload 经外层帧传输，身份内外一致
- 回归：`runtime_wire_identity` 4 用例（v2→v1、v1→v2、name/hash 拒绝、固定布局）；
  sender 身份与校验逻辑均已双向验证（回退后用例失败）
- 同进程跨版本仍走 inproc 类型化指针，不适用解码策略；文档已注明

## 后续整改 P0.2：compat 与版本策略对齐（已完成 2026-11-14）

- 规则：删字段必须「version 递增 + id reserved」；类型/id 变更即使升版本仍 breaking；
  降版本直接错误；`--allow-breaking` 可显式覆盖并记录 WARN
- 修复了「注册表支持多版本、但 CI 禁止发布多版本」的矛盾
- 回归：codegen 自检新增 6 个 compat 用例（同版本删除、升版+reserved、升版无
  reserved、类型变更、allow-breaking、降版本）

## 后续整改 P0.3：录制路径隔离（已完成 2026-11-14）

- `--record-file`（单进程）/ `--record-dir`（多进程，按 `<group>_<ts>.starrec` 生成）；
  未显式指定时单进程默认 `/tmp/starletto_<pid>.starrec`
- 多进程录制未给 `--record-dir` 时**拒绝启动**，不再让多个 worker 截断同一文件
- 实测：planning+control 两个 worker → 两个可 `star record info` 的文件

## 后续整改 P0.4：init 失败语义（已完成 2026-11-14）

- 组件初始化提前到任务注册之前；init/auto_configure 失败 → 标记 failed，不注册任务、
  不进入调度、不调用 shutdown；全部组件失败时 host 抛错、worker 非零退出
- 回归：`runtime_component` 新增 2 用例（混合成功/失败、全部失败）；
  已双向验证（去掉跳过逻辑后失败）
- `star check` 新增 `runtime-smoke`：真实装配并启动/停止一次组件；
  此前单元测试不经过 App::run，漏掉了「context 被 move 后仍被解引用」的装配期崩溃

## 后续整改 P1.2：插件自注册（已完成 2026-11-14）

- 插件入口 `star_register_components(ComponentRegistry*)` 现在同时注册模块 schema
  （通过 `registry->schema_registry()`）；setting/config 与 setting/parameter 两个
  插件都注册（幂等），任一被加载即可用
- `apps/` 删除全部 `#include "modules/..."`：native_runner 不再注册 setting schema；
  record_replay_tool 改为 `--plugin=<lib.so>` 按需加载插件注册表
- `parameter_service_demo` 移入 `modules/setting/parameter/demo/`（它本来就是该模块的
  demo，不应放在 apps）
- `star check` 新增 `app-boundary`（grep 门禁）；新增 `runtime_plugin_schema` 用例
  直接 dlopen 插件并断言 schema/组件已注册；30 套 192 用例、14/14

## 后续整改 P1.1：插件 ABI 契约（第一阶段完成 2026-11-14）

- 新增 `basement/core/build_info.h`：编译器/标准库/ABI 宏/指针宽度/NDEBUG 指纹
- 所有插件 `register.cc` 导出 `star_toolchain_fingerprint()`（脚手架默认生成）
- `doctor --deep` 比对插件与宿主指纹，不一致直接失败；`star check` 的 doctor
  改为 `--deep`，每次门禁都验证插件 ABI
- `docs/plugin_abi.md` 写明当前模型、必须一致项、指纹不覆盖项与后续共享库路线
- **待做（第二阶段）**：抽 `libstar_runtime.so` + C ABI 入口 + 安装头文件/pkg-config

## 后续整改 P1.3：supervisor 重启/退避/readiness（已完成 2026-11-14）

- worker 启动后通过 `STAR_READY_FD` 管道在 host.start() 成功后写 ready 字节；
  supervisor 超时/提前退出判为启动失败（`--ready-timeout-ms`，默认 3000）
- `--restart=on-failure`（默认 never）+ `--max-restarts`（默认 3）+
  `--restart-backoff-ms`（默认 500，指数退避，封顶 32x）
- 重启不覆盖上一次尝试的录制：文件名插 `.attempt<N>`，保留崩溃现场
- 回归：orchestrator 新增 4 用例（退避、ready 成功/无写入/超时）；实测 worker
  超时后按策略重启、预算耗尽后整体失败；正常启动 ready 握手无告警

## 后续整改 P1.4：安全基线（已完成 2026-11-14）

- shm 段 `0600` + attach 时校验属主 uid（`fstat`），拒绝其它用户预建同名段
- 插件库路径只允许模块内相对路径；绝对路径、`..` 越界直接拒绝（host 与 doctor
  共用同一校验）
- 录制文件创建后 `chmod 0600`
- 新增 `docs/security_model.md`：资产、信任边界、威胁/缓解、非目标、部署建议
- 回归：shm 权限、录制权限、插件路径拒绝 3 用例；30 套 199 用例、14/14

## 后续整改 P2.1：conf 严格校验（已完成 2026-11-14）

- PlanLoader 在 `from_value` 前调用 `SchemaValidator::validate(descriptor, root)`：
  - 未知键报错（如 `readres` → `SchemaValidator unknown field: ComponentConf.readres`）
  - 缺失必填、类型不符同样在加载期报错
  - `object` 类型（如 `parameters`）保持不透明，不限制内部键
- 错误经 `build_component_load_plan` 返回，`star run`/`doctor` 直接显示
- 回归：plan 测试新增 1 用例（typo 拒绝 + parameters 通过）；30 套 201 用例

## 后续整改 P2.2：表述修正 + 拓扑类型检查（已完成 2026-11-14）

- README 不再宣称「shm 零拷贝」：明确进程内指针直传、跨进程一次编码+写入、
  固定布局 View 可选但当前默认 TLV 下不适用；component_guide 同步说明
- 新增「资产状态」：`libraries/` 与 motion 的 machine/frame 是保留的算法资产，
  不是死代码；清理时不得按零引用删除
- 新增 `plan_diagnostics.h`：同一通道两端声明的类型不一致在 `doctor` 启动期报错；
  新增 plan 测试（冲突/一致）
- 30 套 202 用例、star check 14/14

## 后续整改 P2.3：随机输入与 sanitizer（已完成 2026-11-14）

- 新增确定性随机测试：binary codec（三种策略）、schema 值解析器、原生 schema
  loader、固定布局帧变异/随机帧；采样次数可用 `STAR_FUZZ_ITERS` 调整
- UBSan 全量跑 32 套用例（`-Db_sanitize=undefined`），发现并修复两处真实 UB：
  1. `FixedLayoutView` 槽位在 packed 26 字节帧头后不满足 8 字节对齐，改为 memcpy 读槽
  2. `BinaryCodec` best-effort 将 NaN/Inf/越界 double 强转 int64；改为范围检查后失败
- ASan 在本机 macOS 27 + Apple clang 上 dyld 初始化死锁（非项目代码），记录在
  docs/testing.md；ASan 交给 Linux CI
- 新增 `.github/workflows/ci.yml`（build/test/check + UBSan job），尚未实际启用
- 32 套 206 用例；正常构建与 UBSan 构建均全绿

## 后续整改 P2.4：拆 app.cc（已完成 2026-11-14）

- `apps/native_runner/app.cc`（1140 行）按职责拆为 5 个 TU：
  - `app_run.cc`（run + multi-process/supervisor）
  - `app_inspect.cc`（list/graph/doctor）
  - `app_selftest.cc`（transport/shm 自检）
  - `app_monitor.cc`（stats/health 订阅与展示）
  - `app_common.cc`（时间戳、路由配置、库解析、日志/统计辅助）
- 纯移动，行为不变；`star check` 14/14、32 套 206 用例、list/monitor/doctor 实跑通过
- **待做**：`Component` 基类拆分（Lifecycle/Ports/Config/Service）与运行时错误模型统一，
  属于 API 变更，放到共享库阶段一起评估

## 后续整改 P1.1 B1：安装面（已完成 2026-11-14）

- `install_subdir` 安装 `basement/` 与 `libraries/` 头，安装脚本删除 `.h/.hpp`
  之外的 meson.build/.schema/Python，避免 include 树变成第二份源码树
- 生成的 builtin `.s.h` 随 custom_target 安装；schema 工具安装到
  `share/starletto/schema`；生成 `starletto.pc`
- 新增 `tools/tests/oot_plugin_smoke.sh`：临时 DESTDIR 安装 → 用安装的 codegen
  生成模块 schema → 源码树外编译插件 → `doctor --deep` → 运行
- `star check` 新增 `install-smoke` 门禁（15/15）
- **待做（B2）**：抽 `libstar_runtime.so` + C ABI 插件入口，放宽跨编译器约束

## 后续整改 P2.5：ResourceMonitor 跨平台（已完成 2026-11-14）

- 原先只有 `/proc` 实现，macOS 上 `sample()` 在 `read_proc_stat` 失败后直接返回全 0：
  `star monitor`/健康检查里的 CPU、RSS、fd、线程数实际是空转。
- macOS 改用 libproc：`PROC_PIDTASKINFO`（RSS/线程）、`PROC_PIDLISTFDS`（fd 数）、
  `proc_name`、`proc_pid_rusage`（CPU 差分）；Linux `/proc` 路径不变。
- 关键坑：`ri_user_time`/`ri_system_time` 是 mach 绝对时钟 tick（本机 24 MHz），
  不是纳秒；直接当 ns 会让 CPU 低报 24 倍，用 `mach_timebase_info` 换算。
  对照 `getrusage` 实测：满核忙循环 94.6%，sleep 0.0%，RSS/fd/线程/name 与 `ps` 一致。
- 首次采样 CPU=0（需两次差分），与 Linux 一致；无权限采样的字段保持 0。
- 回归：新增 `runtime_resource_monitor` 2 用例（自身采样四项非零、未知 pid 全零）；
  33 套 208 用例、`star check` 15/15。
