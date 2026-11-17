# basement/transport —— 通信

传输层只认 `MessageBus`/`Reader`/`Writer` 抽象。组件不直接依赖 shm 实现；
`runtime/context` 负责注入具体总线。

## 目录

| 路径 | 作用 |
|---|---|
| `api/` | `MessageBus`、`ChannelDescriptor`、`ChannelRegistry`、`RecordableBus` |
| `native/` | 进程内总线（typed 指针，不序列化） |
| `shm/` | 共享内存环（4 slot）、seqlock、帧 v7、routed 总线 |
| `record/` | 录制 v2、回放、回放写回通道 |
| `rpc/` | `RpcClient`/`RpcServer`，服务并入普通通道 |

## 命名与协议

- 通道是**数据存放地址**：绝对路径、生产者在前，例如
  `/perception/prediction`、`/localization/pose`、`/control/chassis`、
  `/planning/trajectory`；
- schema 类型名才用点：`perception.prediction`、`planning.trajectory`；
- 服务通道：`<service>/request` / `<service>/response`；
- 文件系统路径是相对的（`modules/planning/motion`）；通道地址是绝对的（前导 `/`），
  一个指向源码文件，一个指向运行期数据；
- `NativeMessageBus`/`ShmMessageBus` 在 `declare_channel` 就校验通道地址：必须 `/` 开头、
  段内 `[A-Za-z0-9_-]`、不允许点；产品与测试都必须用地址；
- shm 槽内必须是 v7 帧（magic/encoding/schema name+version+hash/payload）；
  未加帧或损坏帧会被丢弃、`dropped_count` 递增并记 ERROR；
- 跨进程是编码后拷贝；`ShmSlotView` 的零拷贝读取不用于跨进程交付承诺；
- trigger 消息会在触发线程复制成不可变 `FrameInputs` 快照（shm 载荷），
  保证 `process_once` 看到的就是触发它的那条消息；非 trigger 输入仍走 latest 缓存。

## 测试

`transport_native_message_bus`、`transport_shm_channel`、`transport_shm_routed`、
`transport_shm_stress`、`transport_record_replay`、`transport_replay_publish`、
`transport_rpc_multiprocess`、`transport_service_concurrency`、
`transport_shm_payload_encoding`、`runtime_wire_identity`。
