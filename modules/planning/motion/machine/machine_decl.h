#ifndef __MACHINE_DECL_H__
#define __MACHINE_DECL_H__

// 自动生成的文件，包含所有节点头文件
// 由 header_make.py 脚本生成

#include "motion/machine/nodes/machine_motion.h"
#include "motion/machine/nodes/phase_sl.h"
#include "motion/machine/nodes/phase_ts.h"
#include "motion/machine/nodes/step_lat_decider.h"
#include "motion/machine/nodes/step_lon_decider.h"
#include "motion/machine/nodes/step_rule_lat_decider.h"
#include "motion/machine/nodes/step_rule_lon_decider.h"
#include "motion/machine/nodes/step_sl_data.h"
#include "motion/machine/nodes/step_sldp.h"
#include "motion/machine/nodes/step_slqp.h"
#include "motion/machine/nodes/step_ts_data.h"
#include "motion/machine/nodes/step_tsdp.h"
#include "motion/machine/nodes/step_tsl_combinor.h"
#include "motion/machine/nodes/step_tsqp.h"

namespace star::planning::machine {

inline void force_register_node() {
    // 空函数，仅用于强制编译器包含所有节点头文件，从而确保静态注册变量被初始化
}

} // namespace star::planning::machine

#endif // __MACHINE_DECL_H__