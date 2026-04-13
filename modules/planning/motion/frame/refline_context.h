#ifndef __REFLINE_CONTEXT_H__
#define __REFLINE_CONTEXT_H__

namespace star::planning::data {

struct ReflineContext {
    bool need_global_update = false;
    bool has_lane_info = true;
    // std::vector<ReferenceLine> global_reference_lines;
    // std::vector<ReferenceLine> local_reference_lines;
};

} // namespace star::planning::data

#endif // __REFLINE_CONTEXT_H__
