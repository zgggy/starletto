#ifndef __PIECEWISE_JERK_PROBLEM_H__
#define __PIECEWISE_JERK_PROBLEM_H__

#include <osqp/osqp.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace star::osqp {

/**
 * given x0, x0', x0'', find a goal x0, x1, ..., xn that make the line of x "smooth"
 */

class PiecewiseJerkProblem {
  protected:
    size_t n_;                                                  // number of variables
    double delta_s_;                                            // distance between each point
    std::vector<double> x_;                                     // x0, x1, ..., xn
    std::vector<double> dx_;                                    // x0', x1',..., xn'
    std::vector<double> ddx_;                                   // x0'', x1'',..., xn''
    std::array<double, 3> scale_factor_ = {{1.0, 1.0, 1.0}};    // scale factor for x, dx, ddx
    std::array<double, 3> x_init_;                              // x0, x0', x0'' (initial condition)
    std::vector<std::pair<double, double>> x_bounds_;           // x0, x1,..., xn <lower, upper> bounds
    std::vector<std::pair<double, double>> dx_bounds_;          // x0', x1',..., xn' <lower, upper> bounds
    std::vector<std::pair<double, double>> ddx_bounds_;         // x0'', x1'',..., xn'' <lower, upper> bounds
    std::pair<double, double> dddx_bound_;                      // x0''', x1''',..., xn'' <lower, upper> bounds
    double weight_x_ = 0.0;                                     // weight for x
    double weight_dx_ = 0.0;                                    // weight for dx
    double weight_ddx_ = 0.0;                                   // weight for ddx
    double weight_dddx_ = 0.0;                                  // weight for dddx
    bool has_x_ref_ = false;                                    // if true, use x_ref_ as reference
    std::vector<double> x_ref_;                                 // reference x0, x1,..., xn
    std::vector<double> weight_x_ref_;                          // weight for x_ref_
    bool has_x_n_ref_ = false;                                  // if true, use dx_ref_ as reference
    std::array<double, 3> x_n_ref_;                             // reference x0', x1', x2' (final condition)
    std::array<double, 3> weight_x_n_ref_ = {{0.0, 0.0, 0.0}};  // weight for x_n_ref_
  public:
    PiecewiseJerkProblem(const size_t num_of_points, const double delta_s, const std::array<double, 3>& x_init) : n_(num_of_points), delta_s_(delta_s), x_init_(x_init) {
        x_bounds_.resize(n_, std::make_pair(-1.0e10, 1.0e10));
        dx_bounds_.resize(n_, std::make_pair(-1.0e10, 1.0e10));
        ddx_bounds_.resize(n_, std::make_pair(-1.0e10, 1.0e10));
        dddx_bound_ = std::make_pair(-1.0e10, 1.0e10);
    }
    ~PiecewiseJerkProblem() {}

    void set_scale_factor(const std::array<double, 3>& scale_factor) { scale_factor_ = scale_factor; }

    void set_x_bounds(const std::vector<std::pair<double, double>>& x_bounds) { x_bounds_ = x_bounds; }
    void set_dx_bounds(const std::vector<std::pair<double, double>>& dx_bounds) { dx_bounds_ = dx_bounds; }
    void set_ddx_bounds(const std::vector<std::pair<double, double>>& ddx_bounds) { ddx_bounds_ = ddx_bounds; }
    void set_dddx_bound(const double lower, const double upper) { dddx_bound_ = std::make_pair(lower, upper); }

    void set_weight_x(const double weight_x) { weight_x_ = weight_x; }
    void set_weight_dx(const double weight_dx) { weight_dx_ = weight_dx; }
    void set_weight_ddx(const double weight_ddx) { weight_ddx_ = weight_ddx; }
    void set_weight_dddx(const double weight_dddx) { weight_dddx_ = weight_dddx; }

    void set_x_ref_and_weight(const std::vector<double>& x_ref, const std::vector<double>& weight_x_ref) {
        x_ref_ = x_ref;
        weight_x_ref_ = weight_x_ref;
        has_x_ref_ = true;
    }

    void set_x_n_ref_and_weight(const std::array<double, 3>& x_n_ref, const std::array<double, 3>& weight_x_n_ref) {
        x_n_ref_ = x_n_ref;
        weight_x_n_ref_ = weight_x_n_ref;
        has_x_n_ref_ = true;
    }

    auto opt_x() const -> const std::vector<double>& { return x_; }
    auto opt_dx() const -> const std::vector<double>& { return dx_; }
    auto opt_ddx() const -> const std::vector<double>& { return ddx_; }

    struct OptimizeResult {
        bool result;
        std::string msg;
    };
    virtual auto optimize() -> OptimizeResult {
        auto* settings = new OSQPSettings();
        define_solver_settings(settings);
        osqp_set_default_settings(settings);
        auto [check, P, q, A, l, u, m, n] = formulate_problem();
        if (not check) {
            return OptimizeResult{false, "lower bound is larger than upper bound"};
        }
        OSQPSolver* solver;
        auto status = osqp_setup(&solver, P, q, A, l, u, m, n, settings);

        if (status != OSQP_NO_ERROR) {
            return OptimizeResult{false, "osqp_setup failed"};
        } else {
            status = osqp_solve(solver);
        }

        auto success = true;
        if (status != OSQP_NO_ERROR) {
            success = false;
        } else {
            x_.resize(n_);
            dx_.resize(n_);
            ddx_.resize(n_);
            for (size_t i = 0; i < n_; ++i) {
                x_[i] = solver->solution->x[i];
                dx_[i] = solver->solution->x[n_ + i];
                ddx_[i] = solver->solution->x[2 * n_ + i];
            }
        }

        osqp_cleanup(solver);
        OSQPCscMatrix_free(P);
        OSQPCscMatrix_free(A);
        OSQPSettings_free(settings);

        return success ? OptimizeResult{true, "success"} : OptimizeResult{false, "osqp_solve failed"};
    }

    virtual void define_solver_settings(OSQPSettings* settings) {
        settings->polishing = true;
        settings->verbose = true;
        settings->scaled_termination = true;
        settings->max_iter = 1000;
    }

    virtual auto formulate_problem() -> std::tuple<bool, OSQPCscMatrix*, OSQPFloat*, OSQPCscMatrix*, OSQPFloat*, OSQPFloat*, OSQPInt, OSQPInt> {
        // calculate kernel
        std::vector<OSQPFloat> P_data;
        std::vector<OSQPInt> P_indices;
        std::vector<OSQPInt> P_indptr;
        calculate_kernel(&P_data, &P_indices, &P_indptr);

        // calculate affine constraints
        std::vector<OSQPFloat> A_data;
        std::vector<OSQPInt> A_indices;
        std::vector<OSQPInt> A_indptr;
        std::vector<OSQPFloat> lower_bounds;
        std::vector<OSQPFloat> upper_bounds;
        calculate_affine_constraint(&A_data, &A_indices, &A_indptr, &lower_bounds, &upper_bounds);

        // calculate offset
        std::vector<OSQPFloat> q_data;
        calculate_offset(&q_data);

        size_t kernel_dim = 3 * n_;
        size_t num_affine_constraint = lower_bounds.size();

        auto n = kernel_dim;
        auto m = num_affine_constraint;
        auto P = OSQPCscMatrix_new(kernel_dim, kernel_dim, P_data.size(), copy_data(P_data), copy_data(P_indices), copy_data(P_indptr));
        auto q = copy_data(q_data);
        auto A = OSQPCscMatrix_new(num_affine_constraint, kernel_dim, A_data.size(), copy_data(A_data), copy_data(A_indices), copy_data(A_indptr));
        auto l = copy_data(lower_bounds);
        auto u = copy_data(upper_bounds);

        bool check = check_low_upper_bound(lower_bounds, upper_bounds);

        return std::make_tuple(check, P, q, A, l, u, m, n);
    }

    virtual void calculate_kernel(std::vector<OSQPFloat>* P_data, std::vector<OSQPInt>* P_indices, std::vector<OSQPInt>* P_indptr) = 0;

    virtual void calculate_offset(std::vector<double>* q) = 0;

    virtual void calculate_affine_constraint(std::vector<OSQPFloat>* A_data,        //
                                           std::vector<OSQPInt>* A_indices,       //
                                           std::vector<OSQPInt>* A_indptr,        //
                                           std::vector<OSQPFloat>* lower_bounds,  //
                                           std::vector<OSQPFloat>* upper_bounds) {
        // 3N params bounds on x, x', x''
        // 3(N-1) constraints on x, x', x''
        // 3 constraints on x_init_
        const int num_of_variables = 3 * n_;
        const int num_of_constraints = num_of_variables + 3 * (n_ - 1) + 3;
        lower_bounds->resize(num_of_constraints);
        upper_bounds->resize(num_of_constraints);

        std::vector<std::vector<std::pair<int, double>>> variables(num_of_variables);

        int constraint_index = 0;
        // set x, x', x'' bounds
        for (int i = 0; i < num_of_variables; ++i) {
            if (i < n_) {
                variables[i].emplace_back(constraint_index, 1.0);
                lower_bounds->at(constraint_index) = x_bounds_[i].first * scale_factor_[0];
                upper_bounds->at(constraint_index) = x_bounds_[i].second * scale_factor_[0];
            } else if (i < 2 * n_) {
                variables[i].emplace_back(constraint_index, 1.0);

                lower_bounds->at(constraint_index) = dx_bounds_[i - n_].first * scale_factor_[1];
                upper_bounds->at(constraint_index) = dx_bounds_[i - n_].second * scale_factor_[1];
            } else {
                variables[i].emplace_back(constraint_index, 1.0);
                lower_bounds->at(constraint_index) = ddx_bounds_[i - 2 * n_].first * scale_factor_[2];
                upper_bounds->at(constraint_index) = ddx_bounds_[i - 2 * n_].second * scale_factor_[2];
            }
            ++constraint_index;
        }

        // x(i->i+1)''' = (x(i+1)'' - x(i)'') / delta_s
        for (int i = 0; i + 1 < n_; ++i) {
            variables[2 * n_ + i].emplace_back(constraint_index, -1.0);
            variables[2 * n_ + i + 1].emplace_back(constraint_index, 1.0);
            lower_bounds->at(constraint_index) = dddx_bound_.first * delta_s_ * scale_factor_[2];
            upper_bounds->at(constraint_index) = dddx_bound_.second * delta_s_ * scale_factor_[2];
            ++constraint_index;
        }

        // x(i+1)' - x(i)' - 0.5 * delta_s * x(i)'' - 0.5 * delta_s * x(i+1)'' = 0
        for (int i = 0; i + 1 < n_; ++i) {
            variables[n_ + i].emplace_back(constraint_index, -1.0 * scale_factor_[2]);
            variables[n_ + i + 1].emplace_back(constraint_index, 1.0 * scale_factor_[2]);
            variables[2 * n_ + i].emplace_back(constraint_index, -0.5 * delta_s_ * scale_factor_[1]);
            variables[2 * n_ + i + 1].emplace_back(constraint_index, -0.5 * delta_s_ * scale_factor_[1]);
            lower_bounds->at(constraint_index) = 0.0;
            upper_bounds->at(constraint_index) = 0.0;
            ++constraint_index;
        }

        // x(i+1) - x(i) - delta_s * x(i)'
        // - 1/3 * delta_s^2 * x(i)'' - 1/6 * delta_s^2 * x(i+1)''
        auto delta_s_sq_ = delta_s_ * delta_s_;
        for (int i = 0; i + 1 < n_; ++i) {
            variables[i].emplace_back(constraint_index, -1.0 * scale_factor_[1] * scale_factor_[2]);
            variables[i + 1].emplace_back(constraint_index, 1.0 * scale_factor_[1] * scale_factor_[2]);
            variables[n_ + i].emplace_back(constraint_index, -delta_s_ * scale_factor_[0] * scale_factor_[2]);
            variables[2 * n_ + i].emplace_back(constraint_index, -delta_s_sq_ / 3.0 * scale_factor_[0] * scale_factor_[1]);
            variables[2 * n_ + i + 1].emplace_back(constraint_index, -delta_s_sq_ / 6.0 * scale_factor_[0] * scale_factor_[1]);

            lower_bounds->at(constraint_index) = 0.0;
            upper_bounds->at(constraint_index) = 0.0;
            ++constraint_index;
        }

        // constrain on x_init
        variables[0].emplace_back(constraint_index, 1.0);
        lower_bounds->at(constraint_index) = x_init_[0] * scale_factor_[0];
        upper_bounds->at(constraint_index) = x_init_[0] * scale_factor_[0];
        ++constraint_index;

        variables[n_].emplace_back(constraint_index, 1.0);
        lower_bounds->at(constraint_index) = x_init_[1] * scale_factor_[1];
        upper_bounds->at(constraint_index) = x_init_[1] * scale_factor_[1];
        ++constraint_index;

        variables[2 * n_].emplace_back(constraint_index, 1.0);
        lower_bounds->at(constraint_index) = x_init_[2] * scale_factor_[2];
        upper_bounds->at(constraint_index) = x_init_[2] * scale_factor_[2];
        ++constraint_index;

        int ind_p = 0;
        for (int i = 0; i < num_of_variables; ++i) {
            A_indptr->push_back(ind_p);
            for (const auto& variable_nz : variables[i]) {
                // coefficient
                A_data->push_back(variable_nz.second);

                // constraint index
                A_indices->push_back(variable_nz.first);
                ++ind_p;
            }
        }
        // We indeed need this line because of
        // https://github.com/oxfordcontrol/osqp/blob/master/src/cs.c#L255
        A_indptr->push_back(ind_p);
    }

    bool check_low_upper_bound(std::vector<double>& lower, std::vector<double>& upper) {
        for (size_t i = 0; i < lower.size(); i++) {
            if (lower[i] > upper[i]) {
                return true;
            }
        }
        return false;
    }

    template <typename T>
    T* copy_data(const std::vector<T>& vec) {
        T* data = new T[vec.size()];
        memcpy(data, vec.data(), sizeof(T) * vec.size());
        return data;
    }
};

}  // namespace star::osqp

#endif  // __PIECEWISE_JERK_PROBLEM_H__