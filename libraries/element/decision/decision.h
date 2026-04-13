#ifndef __DECISION_H__
#define __DECISION_H__

#include <string>
#include <vector>
#include <memory>

namespace star::planning {

enum class DecisionType {
    BASE = 0,
    ROUTING,
    REFLINE,
    PATH,
    TRAJ,
    COMPLEX
};

class Decision {
  public:
    explicit Decision(DecisionType type) : type_(type) {}
    virtual ~Decision() = default;

    auto type() const -> DecisionType { return type_; }
    
    template <typename T>
    auto is() const -> bool {
        return type_ == T::kType;
    }

  protected:
    DecisionType type_;
};

class RoutingDecision : public Decision {
  public:
    static constexpr DecisionType kType = DecisionType::ROUTING;
    enum class Type { UNDECIDED, IGNORE, BLOCK, PASSABLE };
    
    explicit RoutingDecision(Type type) 
        : Decision(kType), sub_type_(type) {}
    
    auto sub_type() const -> Type { return sub_type_; }

  private:
    Type sub_type_;
};

class ReflineDecision : public Decision {
  public:
    static constexpr DecisionType kType = DecisionType::REFLINE;
    enum class Type { UNDECIDED, IGNORE, KEEP, CHANGE, BORROW };
    
    explicit ReflineDecision(Type type) 
        : Decision(kType), sub_type_(type) {}
    
    auto sub_type() const -> Type { return sub_type_; }

  private:
    Type sub_type_;
};

class PathDecision : public Decision {
  public:
    static constexpr DecisionType kType = DecisionType::PATH;
    enum class Type { 
        UNDECIDED, 
        IGNORE, 
        LEFT_NUDGE,
        RIGHT_NUDGE,
        LEFT_DETOUR,
        RIGHT_DETOUR,
        LEFT_BORROW,
        RIGHT_BORROW,
        LEFT_CHANGE,
        RIGHT_CHANGE,
        STOP 
    };
    
    explicit PathDecision(Type type) 
        : Decision(kType), sub_type_(type) {}
    
    auto sub_type() const -> Type { return sub_type_; }

  private:
    Type sub_type_;
};

// Traj Decisions
class TrajDecision : public Decision {
  public:
    static constexpr DecisionType kType = DecisionType::TRAJ;
    enum class Type { UNDECIDED, IGNORE, STOP, FOLLOW, YIELD, OVERTAKE };
    
    explicit TrajDecision(Type type) 
        : Decision(kType), sub_type_(type) {}
    
    auto sub_type() const -> Type { return sub_type_; }

  private:
    Type sub_type_;
};

// Complex Decisions
class ComplexDecision : public Decision {
  public:
    static constexpr DecisionType kType = DecisionType::COMPLEX;
    enum class Type { NONE, MEETING, PASSING };
    
    explicit ComplexDecision(Type type) 
        : Decision(kType), sub_type_(type) {}
    
    auto sub_type() const -> Type { return sub_type_; }
    
    auto decompose() const -> std::vector<std::shared_ptr<Decision>> {
        std::vector<std::shared_ptr<Decision>> decisions;
        if (sub_type_ == Type::MEETING) {
            decisions.push_back(std::make_shared<PathDecision>(PathDecision::Type::RIGHT_BORROW));
            decisions.push_back(std::make_shared<TrajDecision>(TrajDecision::Type::YIELD));
        } else if (sub_type_ == Type::PASSING) {
            decisions.push_back(std::make_shared<PathDecision>(PathDecision::Type::LEFT_CHANGE));
            decisions.push_back(std::make_shared<TrajDecision>(TrajDecision::Type::OVERTAKE));
        }
        return decisions;
    }

  private:
    Type sub_type_;
};

}  // namespace star::planning

#endif  // __DECISION_H__
