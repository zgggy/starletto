# libraries — 算法资产层

按 `00_hub/05_docs/spec/system_vision.md` §3.2 的定义：纯 C++ 算法与数据结构层，
**禁止依赖 `transport`/`runtime`**，只依赖 `basement/core` 与必要的数学/几何库。

## ⚠️ 当前状态：整层未接线

经全库引用核查（2026-09-26），**本层尚无任何活代码消费者**：

- 活模块（`modules/control/pid`、`planning/motion`、`setting/*`）只依赖 `basement/` 与 `modules/setting/schema/`
- 本层的唯一引用来自另外两组**同样未接线**的骨架：`modules/planning/motion/frame/`、`modules/planning/motion/machine/`

因此：本层代码**不参与任何构建目标**，也不被 `star build`/`star test` 编译。这是规划中的资产，
对应路线图里尚未落地的 `localization` / `perception` / `prediction` / `planning` 业务模块。

各子目录现状：

| 子目录 | 规模 | 内容 | 状态 |
|---|---|---|---|
| `common/math/` | 7 文件 / 870 行 | 多项式、螺旋线、贝塞尔、凸包、OBB、插值 | 未接线 |
| `common/blackboard/` | 1 文件 / 156 行 | 线程安全键值黑板（行为树节点间共享数据） | 未接线 |
| `element/` | 28 文件 / 1030 行 | 领域类型：Frenet、ST 图、ReferenceLine、轨迹、决策层次 | 未接线 |
| `machine/` | 9 文件 / 699 行 | 行为树框架（`BaseNode`/`UnitNode`/`NodeFactory`） | 未接线 |
| `method/` | 16 文件 / 900 行 | A*/Dijkstra/Bellman-Ford、Hybrid A*、piecewise jerk（**依赖 OSQP**） | 未接线 |
| `tool/` | 1 文件 / 14 行 | 格式化辅助 | 未接线 |
| `third_party/` | 2 个单头 | `nlohmann/json.hpp`、`toml/toml.hpp` | 供上层使用 |

## 接线前需要注意的两点

1. **`method/algorithm/piecewise_jerk/` 依赖 OSQP**（`#include <osqp/osqp.h>`）。
   meson 目前**没有** OSQP 依赖声明；接入前需先解决依赖引入方式。
2. **`method/adaptor/path_calculation.h` 是未实现的骨架**，桩函数已补 `return` 以消除
   「调用即 UB」的空体非 void 形状，但整体仍未完成（`Point` 缺少 `x()`/`y()` 等）。

## 接线方式建议

接入时请让业务模块通过 `libraries_dep` 显式声明依赖，并把用到的头加入构建目标——
保持「本层不依赖 transport/runtime」的硬约束（见 spec）。
