#ifndef __PATH_CALCULATION_H__
#define __PATH_CALCULATION_H__

/**
 * calculation type adaptor call osqp to calculate the path
 */

#include <string>

#include "element/elements.h"
#include "method/algorithm/piecewise_jerk/piecewise_jerk_path_problem.h"

namespace star::method {

class Point {};
class PathPoint {
  public:
    Point pose() const {};
};
class Path {
  public:
    auto xy_to_s(const double& x, const double& y) const -> double {}
    auto s_to_point(const double& s) const -> Point {}
    auto length() const -> double {}
};
class Obstacle {};
class VehicleConfig {
  public:
    double left_to_center() const {}
    double right_to_center() const {}
};
class PathBounds : public std::vector<std::tuple<double, double, double>> {
  public:
    double delta_s() const {}
};
class TrajPoint {
  public:
    Point pose() const {};
};

class PathCalculation {
  private:
    PathCalculation() {}

    auto optimize_path(const Path& reference_line, std::vector<Obstacle> obstacles, const VehicleConfig& vehicle_config, const TrajPoint& init_state, double max_distance, size_t max_iteration,
                      std::vector<double>* s, std::vector<double>* l, std::vector<double>* dl, std::vector<double>* ddl) -> bool {
        // step 1, update the path bound
        auto path_bounds = PathBounds{};
        const auto ref_s = reference_line.xy_to_s(init_state.pose().x(), init_state.pose().y());
        const auto ref_point = reference_line.s_to_point(ref_s);
        update_path_bounds(reference_line, obstacles,                                                                                                    //
                         ref_s, std::fmin(reference_line.length(), max_distance), vehicle_config.left_to_center() + vehicle_config.right_to_center(),  //
                         &path_bounds);

        // step 2, init the piecewise jerk problem
        const auto num_of_knots = path_bounds.size();
        if (num_of_knots < 2) {
            logE << "num of point less than 2.";
            return false;
        }
        const auto delta_s = path_bounds.delta_s();
        if (delta_s < 1e-6) {
            logE << "delta_s is 0, maybe not initialized.";
            return false;
        }
        std::array<double, 3> init_s, init_l;
        Math::CartesianToFrenet(ref_point.s(), ref_point.x(), ref_point.y(), ref_point.theta(), ref_point.kappa(), ref_point.dkappa(),                     //
                                init_state.x(), init_state.y(), init_state.theta(), init_state.kappa(), init_state.velocity(), init_state.acceleration(),  //
                                &init_s, &init_l);

        auto piecewise_jerk_problem = star::osqp::PiecewiseJerkPathProblem{num_of_knots, delta_s, init_l};

        // step 3, setup the piecewise jerk problem
        const auto [updated_weight_l_ref, l_ref] = cal_l_ref(path_bounds, config.weight_l_ref());
        auto path_bounds_s = std::vector<double>{};
        auto path_bounds_without_s = std::vector<std::pair<double, double>>{};
        for (const auto& point : path_bounds) {
            path_bounds_s.push_back(std::get<0>(point));
            path_bounds_without_s.push_back(std::make_pair(std::get<1>(point), std::get<2>(point)));
        }
        auto path_dl_bounds = std::vector<std::pair<double, double>>{};
        auto path_ddl_bounds = std::vector<std::pair<double, double>>{};
        auto path_dddl_bounds_lower = double{};
        auto path_dddl_bounds_upper = double{};

        piecewise_jerk_problem.set_x_ref_and_weight(updated_weight_l_ref, l_ref);
        piecewise_jerk_problem.set_x_n_ref_and_weight({1000, 0, 0}, {0, 0, 0});
        piecewise_jerk_problem.set_weight_x(10.0);
        piecewise_jerk_problem.set_weight_dx(20.0);
        piecewise_jerk_problem.set_weight_ddx(50.0);
        piecewise_jerk_problem.set_weight_dddx(500.0);
        piecewise_jerk_problem.set_scale_factor({1.0, 10.0, 100.0});
        piecewise_jerk_problem.set_x_bounds(path_bounds_without_s);
        piecewise_jerk_problem.set_dx_bounds(path_dl_bounds);
        piecewise_jerk_problem.set_ddx_bounds(path_ddl_bounds);
        piecewise_jerk_problem.set_dddx_bound(path_dddl_bounds_lower, path_dddl_bounds_upper);

        // step 4, optimize the problem
        auto res = piecewise_jerk_problem.optimize();
        if (not res.result) {
            logE << res.msg;
            return false;
        }
        *s = path_bounds_s;
        *l = piecewise_jerk_problem.opt_x();
        *dl = piecewise_jerk_problem.opt_dx();
        *ddl = piecewise_jerk_problem.opt_ddx();
        return true;
    }

    bool update_path_bounds(const Path& reference_line, std::vector<Obstacle> obstacles, double start_s, double max_s, double vehicle_width, PathBounds* path_bounds) {}

    auto cal_l_ref(const PathBounds& path_bounds, double default_weight_l_ref) -> std::tuple<std::vector<double>, std::vector<double>> {}
};

}  // namespace star::method

#endif  // __PATH_CALCULATION_H__