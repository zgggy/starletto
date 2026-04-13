#ifndef __DECISION_BUILDER_H__
#define __DECISION_BUILDER_H__

#include <memory>
#include <string>
#include <vector>
#include <map>
#include "../decision/decision.h"
#include "../container/decision_info.h"

namespace star::planning {

// 决策规则配置
struct DecisionConfig {
    bool enable_lazy_evaluation = false;
    bool enable_historical_consistency = true;
    float default_nudge_buffer = 0.5f;
    float default_stop_distance = 2.0f;
    
    // 冲突处理优先级: 值越小优先级越高
    std::map<DecisionType, int> type_priority = {
        {DecisionType::COMPLEX, 1},
        {DecisionType::TRAJ, 2},
        {DecisionType::PATH, 3},
        {DecisionType::REFLINE, 4},
        {DecisionType::ROUTING, 5}
    };
};

// 预处理后的障碍物决策辅助数据
struct ObstacleDecisionData {
    uint32_t id;
    float speed;
    float distance_to_path;
    float safe_buffer;
    bool is_static;
};

class DecisionBuilder {
  public:
    explicit DecisionBuilder(const DecisionConfig& config) : config_(config) {}
    ~DecisionBuilder() = default;

    // 为 Decider 节点提供障碍物决策所需的辅助数据
    auto prepare_obstacle_decision_data(uint32_t obstacle_id) -> ObstacleDecisionData {
        if (config_.enable_lazy_evaluation) {
            // 如果启用延迟构建，这里可以只返回基础 ID，具体计算在需要时触发
        }
        
        ObstacleDecisionData data;
        data.id = obstacle_id;
        data.safe_buffer = config_.default_nudge_buffer;
        return data;
    }

    // 构建决策并处理冲突
    void build_and_merge_decision(uint32_t obstacle_id, 
                               std::shared_ptr<Decision> decision, 
                               DecisionInfo* current_info,
                               const DecisionInfo* last_info = nullptr) {
        if (not current_info) return;

        std::vector<std::shared_ptr<Decision>> decisions_to_add;
        
        // 1. 处理复杂决策分解
        if (decision->type() == DecisionType::COMPLEX) {
            auto complex = std::static_pointer_cast<ComplexDecision>(decision);
            decisions_to_add = complex->decompose();
            decisions_to_add.push_back(decision);
        } else {
            decisions_to_add.push_back(decision);
        }

        // 2. 历史一致性处理
        if (config_.enable_historical_consistency and last_info) {
            apply_historical_consistency(obstacle_id, decisions_to_add, *last_info);
        }

        // 3. 冲突处理与存储
        for (const auto& d : decisions_to_add) {
            if (not has_conflict(obstacle_id, d, *current_info)) {
                current_info->set_decision(obstacle_id, d);
            }
        }
    }

  private:
    // 历史一致性逻辑：如果当前决策与上一帧冲突且差异不大，倾向于维持上一帧
    void apply_historical_consistency(uint32_t obstacle_id, 
                                    std::vector<std::shared_ptr<Decision>>& current_decisions, 
                                    const DecisionInfo& last_info) {
        const auto* last_decisions = last_info.get_decisions(obstacle_id);
        if (not last_decisions) return;

        for (auto& current : current_decisions) {
            for (const auto& last : *last_decisions) {
                if (current->type() == last->type()) {
                    // 示例：如果上一帧是 STOP，当前帧是 IGNORE，且障碍物仍在安全范围内，可能需要维持 STOP
                    if (last->type() == DecisionType::PATH) {
                        auto last_p = std::static_pointer_cast<PathDecision>(last);
                        auto curr_p = std::static_pointer_cast<PathDecision>(current);
                        if (last_p->sub_type() == PathDecision::Type::STOP and 
                            curr_p->sub_type() == PathDecision::Type::IGNORE) {
                            current = last; 
                        }
                    } else if (last->type() == DecisionType::TRAJ) {
                        auto last_t = std::static_pointer_cast<TrajDecision>(last);
                        auto curr_t = std::static_pointer_cast<TrajDecision>(current);
                        if (last_t->sub_type() == TrajDecision::Type::STOP and 
                            curr_t->sub_type() == TrajDecision::Type::IGNORE) {
                            current = last; 
                        }
                    }
                }
            }
        }
    }

    // 冲突检测逻辑
    auto has_conflict(uint32_t obstacle_id, 
                     std::shared_ptr<Decision> new_decision, 
                     const DecisionInfo& current_info) -> bool {
        const auto* existing = current_info.get_decisions(obstacle_id);
        if (not existing) return false;

        for (const auto& d : *existing) {
            // 同一阶段、同一类型的决策冲突处理
            if (d->type() == new_decision->type()) {
                // 根据优先级决定是否冲突
                int existing_prio = config_.type_priority.at(d->type());
                int new_prio = config_.type_priority.at(new_decision->type());
                if (new_prio >= existing_prio) return true; // 已有高优先级决策
            }
        }
        return false;
    }

    DecisionConfig config_;
};

}  // namespace star::planning

#endif  // __DECISION_BUILDER_H__
