#ifndef __DECISION_INFO_H__
#define __DECISION_INFO_H__

#include "../decision/decision.h"
#include "basement/core/logger/logger.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace star::planning {

class DecisionInfo {
  public:
    DecisionInfo() = default;
    ~DecisionInfo() = default;

    // 为障碍物设置决策
    void set_decision(uint32_t obstacle_id, std::shared_ptr<Decision> decision) {
        decisions_[obstacle_id].push_back(decision);
    }

    // 获取特定障碍物的所有决策
    auto get_decisions(uint32_t obstacle_id) const -> const std::vector<std::shared_ptr<Decision>>* {
        auto it = decisions_.find(obstacle_id);
        if (it != decisions_.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    // 检查障碍物是否具有特定类型的决策
    auto has_decision_type(uint32_t obstacle_id, DecisionType type) const -> bool {
        auto it = decisions_.find(obstacle_id);
        if (it == decisions_.end()) {
            return false;
        }
        
        return std::any_of(it->second.begin(), it->second.end(), [type](const auto& d) {
            return d->type() == type;
        });
    }

    // 清理所有决策
    void clear() {
        decisions_.clear();
    }

    // 检查进入阶段前，是否所有给定的障碍物都至少有一个指定类型的决策
    auto check_decision_type_full_coverage(DecisionType type, const std::vector<uint32_t>& obstacle_ids) const -> bool {
        for (uint32_t id : obstacle_ids) {
            auto it = decisions_.find(id);
            if (it == decisions_.end()) {
                logW << "Obstacle [" << id << "] has no decisions.";
                return false;
            }

            const bool covered = has_decision_type(id, type);

            if (not covered) {
                logW << "Obstacle [" << id << "] has no decision of type [" << static_cast<int>(type) << "]";
                return false;
            }
        }
        return true;
    }

    // 获取所有决策映射
    auto all_decisions() const -> const std::unordered_map<uint32_t, std::vector<std::shared_ptr<Decision>>>& {
        return decisions_;
    }

  private:
    std::unordered_map<uint32_t, std::vector<std::shared_ptr<Decision>>> decisions_;
};

}  // namespace star::planning

#endif  // __DECISION_INFO_H__
