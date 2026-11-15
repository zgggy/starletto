# perception.prediction（stub）

上游假节点：按 50ms 周期发布一条合成预测到 `/perception/prediction`。
只用于让 planning 的 SyncTrigger 与数据链路先跑起来，不包含感知算法。

- 组件类：`star::perception::PredictionComponent`
- 输出：`Prediction{timestamp_ns, objects[]}`；当前固定一个前方目标
- schema：`modules/setting/schema/perception/prediction.schema`
