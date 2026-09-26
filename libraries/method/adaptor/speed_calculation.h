#ifndef __SPEED_CALCULATION_H__
#define __SPEED_CALCULATION_H__

/**
 * calculation type adaptor call osqp to calculate the speed
 */

#include "method/algorithm/piecewise_jerk/piecewise_jerk_problem.h"

namespace star::method {

class SpeedCalculation : public star::osqp::PiecewiseJerkProblem {
  protected:
    bool has_dx_ref_ = false;
    std::vector<double> dx_ref_;
    std::vector<double> weight_dx_ref_;
    std::vector<double> penalty_dx_;

  public:
    SpeedCalculation(const size_t num_of_knots, const double delta_s, const std::array<double, 3>& x_init) : PiecewiseJerkProblem(num_of_knots, delta_s, x_init) {
        dx_ref_.resize(num_of_knots, 0.0);
        weight_dx_ref_.resize(num_of_knots, 0.0);
        penalty_dx_.resize(num_of_knots, 0.0);
    }
    virtual ~SpeedCalculation() {}

    void set_dx_ref_and_weight(const std::vector<double>& dx_ref, const std::vector<double>& weight_dx_ref) {
        weight_dx_ref_ = weight_dx_ref;
        dx_ref_ = dx_ref;
        has_dx_ref_ = true;
    }
    void set_penalty_dx(const std::vector<double>& penalty_dx) { penalty_dx_ = penalty_dx; }

    void calculate_kernel(std::vector<OSQPFloat>* P_data, std::vector<OSQPInt>* P_indices, std::vector<OSQPInt>* P_indptr) override {
        const int n = static_cast<int>(n_);
        const int kNumParam = 3 * n;
        const int kNumValue = 4 * n - 1;
        std::vector<std::vector<std::pair<OSQPInt, OSQPFloat>>> columns;
        columns.resize(kNumParam);
        int value_index = 0;

        // x(i)^2 * w_x_ref
        for (int i = 0; i < n - 1; ++i) {
            columns[i].emplace_back(i, weight_x_ref_[i] / (scale_factor_[0] * scale_factor_[0]));
            ++value_index;
        }
        // x(n-1)^2 * (w_x_ref + w_end_x)
        columns[n - 1].emplace_back(n - 1, (weight_x_ref_[n - 1] + weight_x_n_ref_[0]) / (scale_factor_[0] * scale_factor_[0]));
        ++value_index;

        // x(i)'^2 * (w_dx_ref + penalty_dx)
        for (int i = 0; i < n - 1; ++i) {
            columns[n + i].emplace_back(n + i, (weight_dx_ref_[i] + penalty_dx_[i]) / (scale_factor_[1] * scale_factor_[1]));
            ++value_index;
        }
        // x(n-1)'^2 * (w_dx_ref + penalty_dx + w_end_dx)
        columns[2 * n - 1].emplace_back(2 * n - 1, (weight_dx_ref_[n - 1] + penalty_dx_[n - 1] + weight_x_n_ref_[1]) / (scale_factor_[1] * scale_factor_[1]));
        ++value_index;

        auto delta_s_square = delta_s_ * delta_s_;
        // x(i)''^2 * (w_ddx + 2 * w_dddx / delta_s^2)
        columns[2 * n].emplace_back(2 * n, (weight_ddx_ + weight_dddx_ / delta_s_square) / (scale_factor_[2] * scale_factor_[2]));
        ++value_index;

        for (int i = 1; i < n - 1; ++i) {
            columns[2 * n + i].emplace_back(2 * n + i, (weight_ddx_ + 2.0 * weight_dddx_ / delta_s_square) / (scale_factor_[2] * scale_factor_[2]));
            ++value_index;
        }

        columns[3 * n - 1].emplace_back(3 * n - 1, (weight_ddx_ + weight_dddx_ / delta_s_square + weight_x_n_ref_[2]) / (scale_factor_[2] * scale_factor_[2]));
        ++value_index;

        // -2 * w_dddx / delta_s^2 * x(i)'' * x(i + 1)''
        for (int i = 0; i < n - 1; ++i) {
            columns[2 * n + i].emplace_back(2 * n + i + 1, -2.0 * weight_dddx_ / delta_s_square / (scale_factor_[2] * scale_factor_[2]));
            ++value_index;
        }

        int ind_p = 0;
        for (int i = 0; i < kNumParam; ++i) {
            P_indptr->push_back(ind_p);
            for (const auto& row_data_pair : columns[i]) {
                P_data->push_back(row_data_pair.second * 2.0);
                P_indices->push_back(row_data_pair.first);
                ++ind_p;
            }
        }
        P_indptr->push_back(ind_p);
    }

    void calculate_offset(std::vector<double>* q) override {
        const int n = static_cast<int>(n_);
        const int kNumParam = 3 * n;
        q->resize(kNumParam);
        for (int i = 0; i < n; ++i) {
            if (has_x_ref_) {
                q->at(i) += -2.0 * weight_x_ref_[i] * x_ref_[i] / scale_factor_[0];
            }
            if (has_dx_ref_) {
                q->at(n + i) += -2.0 * weight_dx_ref_[i] * dx_ref_[i] / scale_factor_[1];
            }
        }

        if (has_x_n_ref_) {
            q->at(n - 1) += -2.0 * weight_x_n_ref_[0] * x_n_ref_[0] / scale_factor_[0];
            q->at(2 * n - 1) += -2.0 * weight_x_n_ref_[1] * x_n_ref_[1] / scale_factor_[1];
            q->at(3 * n - 1) += -2.0 * weight_x_n_ref_[2] * x_n_ref_[2] / scale_factor_[2];
        }
    }
};

}  // namespace star::method

#endif  // __SPEED_CALCULATION_H__