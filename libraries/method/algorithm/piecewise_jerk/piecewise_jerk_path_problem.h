#ifndef __PIECEWISW_JERK_PATH_PROBLEM__
#define __PIECEWISW_JERK_PATH_PROBLEM__

/**
 * the path problem
 */

#include "piecewise_jerk_problem.h"

namespace star::osqp {

class PiecewiseJerkPathProblem : public star::osqp::PiecewiseJerkProblem {
  public:
    PiecewiseJerkPathProblem(const size_t num_of_knots, const double delta_s, const std::array<double, 3>& x_init) : PiecewiseJerkProblem(num_of_knots, delta_s, x_init) {}

    virtual ~PiecewiseJerkPathProblem() = default;

  protected:
    void calculate_kernel(std::vector<OSQPFloat>* P_data, std::vector<OSQPInt>* P_indices, std::vector<OSQPInt>* P_indptr) override {
        const int num_of_variables = 3 * n_;
        const int num_of_nonzeros = num_of_variables + (n_ - 1);
        std::vector<std::vector<std::pair<OSQPInt, OSQPFloat>>> columns(num_of_variables);
        int value_index = 0;

        // x(i)^2 * (w_x + w_x_ref[i]), w_x_ref might be a uniform value for all x(i)
        // or piecewise values for different x(i)
        for (int i = 0; i < n_ - 1; ++i) {
            columns[i].emplace_back(i, (weight_x_ + weight_x_ref_[i]) / (scale_factor_[0] * scale_factor_[0]));
            ++value_index;
        }
        // x(n_-1)^2 * (w_x + w_x_ref[n_-1] + w_end_x)
        columns[n_ - 1].emplace_back(n_ - 1, (weight_x_ + weight_x_ref_[n_ - 1] + weight_x_n_ref_[0]) / (scale_factor_[0] * scale_factor_[0]));
        ++value_index;

        // x(i)'^2 * w_dx
        for (int i = 0; i < n_ - 1; ++i) {
            columns[n_ + i].emplace_back(n_ + i, weight_dx_ / (scale_factor_[1] * scale_factor_[1]));
            ++value_index;
        }
        // x(n_-1)'^2 * (w_dx + w_end_dx)
        columns[2 * n_ - 1].emplace_back(2 * n_ - 1, (weight_dx_ + weight_x_n_ref_[1]) / (scale_factor_[1] * scale_factor_[1]));
        ++value_index;

        auto delta_s_square = delta_s_ * delta_s_;
        // x(i)''^2 * (w_ddx + 2 * w_dddx / delta_s^2)
        columns[2 * n_].emplace_back(2 * n_, (weight_ddx_ + weight_dddx_ / delta_s_square) / (scale_factor_[2] * scale_factor_[2]));
        ++value_index;
        for (int i = 1; i < n_ - 1; ++i) {
            columns[2 * n_ + i].emplace_back(2 * n_ + i, (weight_ddx_ + 2.0 * weight_dddx_ / delta_s_square) / (scale_factor_[2] * scale_factor_[2]));
            ++value_index;
        }
        columns[3 * n_ - 1].emplace_back(3 * n_ - 1, (weight_ddx_ + weight_dddx_ / delta_s_square + weight_x_n_ref_[2]) / (scale_factor_[2] * scale_factor_[2]));
        ++value_index;

        // // -2 * w_dddx / delta_s^2 * x(i)'' * x(i + 1)''
        // for (int i = 0; i < n_ - 1; ++i) {
        //     columns[2 * n_ + i].emplace_back(2 * n_ + i + 1, (-2.0 * weight_dddx_ / delta_s_square) / (scale_factor_[2] * scale_factor_[2]));
        //     ++value_index;
        // }

        int ind_p = 0;
        for (int i = 0; i < num_of_variables; ++i) {
            P_indptr->push_back(ind_p);
            for (const auto& row_data_pair : columns[i]) {
                P_data->push_back(row_data_pair.second * 2.0);
                P_indices->push_back(row_data_pair.first);
                ++ind_p;
            }
        }
        P_indptr->push_back(ind_p);
    }

    void calculate_offset(std::vector<OSQPFloat>* q) override {
        q->resize(3 * n_, 0.0);
        if (has_x_ref_) {
            for (int i = 0; i < n_; ++i) {
                q->at(i) += -2.0 * weight_x_ref_.at(i) * x_ref_[i] / scale_factor_[0];
            }
        }
        if (has_x_n_ref_) {
            q->at(n_ - 1) += -2.0 * weight_x_n_ref_[0] * x_n_ref_[0] / scale_factor_[0];
            q->at(2 * n_ - 1) += -2.0 * weight_x_n_ref_[1] * x_n_ref_[1] / scale_factor_[1];
            q->at(3 * n_ - 1) += -2.0 * weight_x_n_ref_[2] * x_n_ref_[2] / scale_factor_[2];
        }
    }
};

}  // namespace star::osqp

#endif  // __PIECEWISW_JERK_PATH_PROBLEM__