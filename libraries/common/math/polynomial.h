#ifndef __POLYNOMIAL_H__
#define __POLYNOMIAL_H__

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace star::common::math {

class Polynomial {
  public:
    std::vector<double> coeffs;

    Polynomial() = default;
    explicit Polynomial(const std::vector<double>& c) : coeffs(c) {}

    static auto from_boundary_conditions(double t0, double t1, const std::vector<double>& values_t0, const std::vector<double>& values_t1) -> Polynomial {
        size_t n = values_t0.size();
        if (values_t1.size() != n) {
            throw std::invalid_argument("values_t0 and values_t1 must have the same size");
        }
        size_t order = 2 * n - 1;
        std::vector<double> coeffs(order + 1, 0.0);

        std::vector<std::vector<double>> A(order + 1, std::vector<double>(order + 1, 0.0));
        std::vector<double> b(order + 1, 0.0);

        for (size_t k = 0; k < n; ++k) {
            for (size_t j = k; j <= order; ++j) {
                double coeff = 1.0;
                for (size_t m = 0; m < k; ++m) {
                    coeff *= (j - m);
                }
                double t_pow = 1.0;
                for (size_t m = 0; m < k; ++m) {
                    t_pow *= t0;
                }
                A[k][j] = coeff * t_pow;
            }
            b[k] = values_t0[k];
        }

        for (size_t k = 0; k < n; ++k) {
            for (size_t j = k; j <= order; ++j) {
                double coeff = 1.0;
                for (size_t m = 0; m < k; ++m) {
                    coeff *= (j - m);
                }
                double t_pow = 1.0;
                for (size_t m = 0; m < k; ++m) {
                    t_pow *= t1;
                }
                A[n + k][j] = coeff * t_pow;
            }
            b[n + k] = values_t1[k];
        }

        auto x = solve_linear_system(A, b);
        return Polynomial(x);
    }

    static auto cubic_spline_segment(double x0, double x1, double y0, double y1, double dy0, double dy1) -> Polynomial {
        double h = x1 - x0;
        std::vector<double> a(4);
        a[0] = y0;
        a[1] = dy0;
        a[2] = (3.0 * (y1 - y0) / h - 2.0 * dy0 - dy1) / h;
        a[3] = (2.0 * (y0 - y1) / h + dy0 + dy1) / (h * h);
        return Polynomial(a);
    }

    static auto quintic_spline_segment(double t0, double t1, double y0, double y1, double dy0, double dy1, double ddy0, double ddy1) -> Polynomial {
        double h = t1 - t0;
        double h2 = h * h;
        double h3 = h2 * h;
        double h4 = h3 * h;
        double h5 = h4 * h;

        std::vector<double> a(6);
        a[0] = y0;
        a[1] = dy0;
        a[2] = ddy0 / 2.0;

        double delta_y = y1 - y0;
        double delta_dy = dy1 - dy0;
        double delta_ddy = ddy1 - ddy0;

        a[3] = (20.0 * delta_y - (8.0 * dy1 + 12.0 * dy0) * h - (3.0 * ddy0 - ddy1) * h2) / (2.0 * h3);
        a[4] = (-30.0 * delta_y + (14.0 * dy1 + 16.0 * dy0) * h + (3.0 * ddy0 - 2.0 * ddy1) * h2) / (2.0 * h4);
        a[5] = (12.0 * delta_y - 6.0 * (dy1 + dy0) * h - (ddy0 - ddy1) * h2) / (2.0 * h5);

        return Polynomial(a);
    }

    auto eval(double x) const -> double {
        double result = 0.0;
        double x_pow = 1.0;
        for (size_t i = 0; i < coeffs.size(); ++i) {
            result += coeffs[i] * x_pow;
            x_pow *= x;
        }
        return result;
    }

    auto derivative() const -> Polynomial {
        if (coeffs.size() <= 1) {
            return Polynomial({0.0});
        }
        std::vector<double> new_coeffs;
        new_coeffs.reserve(coeffs.size() - 1);
        for (size_t i = 1; i < coeffs.size(); ++i) {
            new_coeffs.push_back(coeffs[i] * static_cast<double>(i));
        }
        return Polynomial(new_coeffs);
    }

    auto integral(double c = 0.0) const -> Polynomial {
        std::vector<double> new_coeffs;
        new_coeffs.reserve(coeffs.size() + 1);
        new_coeffs.push_back(c);
        for (size_t i = 0; i < coeffs.size(); ++i) {
            new_coeffs.push_back(coeffs[i] / static_cast<double>(i + 1));
        }
        return Polynomial(new_coeffs);
    }

    auto roots(double epsilon = 1e-10) const -> std::vector<double> {
        std::vector<double> result;
        if (coeffs.empty()) {
            return result;
        }
        if (coeffs.size() == 1) {
            if (std::fabs(coeffs[0]) < epsilon) {
                result.push_back(0.0);
            }
            return result;
        }
        if (coeffs.size() == 2) {
            if (std::fabs(coeffs[1]) > epsilon) {
                result.push_back(-coeffs[0] / coeffs[1]);
            }
            return result;
        }
        if (coeffs.size() == 3) {
            double a = coeffs[2];
            double b = coeffs[1];
            double c = coeffs[0];
            double delta = b * b - 4.0 * a * c;
            if (delta >= -epsilon) {
                double sqrt_delta = std::sqrt(std::max(delta, 0.0));
                if (std::fabs(delta) < epsilon) {
                    result.push_back(-b / (2.0 * a));
                } else {
                    result.push_back((-b - sqrt_delta) / (2.0 * a));
                    result.push_back((-b + sqrt_delta) / (2.0 * a));
                }
            }
            return result;
        }
        auto deriv = derivative();
        auto critical_points = deriv.roots(epsilon);
        critical_points.push_back(-1e10);
        critical_points.push_back(1e10);
        std::sort(critical_points.begin(), critical_points.end());
        for (size_t i = 0; i < critical_points.size() - 1; ++i) {
            double left = critical_points[i];
            double right = critical_points[i + 1];
            double f_left = eval(left);
            double f_right = eval(right);
            if (f_left * f_right <= epsilon) {
                for (int iter = 0; iter < 100; ++iter) {
                    double mid = (left + right) / 2.0;
                    double f_mid = eval(mid);
                    if (std::fabs(f_mid) < epsilon  or  (right - left) < epsilon) {
                        if (result.empty()  or  std::fabs(mid - result.back()) > epsilon) {
                            result.push_back(mid);
                        }
                        break;
                    }
                    if (f_mid * f_left < 0.0) {
                        right = mid;
                        f_right = f_mid;
                    } else {
                        left = mid;
                        f_left = f_mid;
                    }
                }
            }
        }
        return result;
    }

  private:
    static auto solve_linear_system(const std::vector<std::vector<double>>& A, const std::vector<double>& b) -> std::vector<double> {
        size_t n = A.size();
        std::vector<std::vector<double>> aug(n, std::vector<double>(n + 1));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                aug[i][j] = A[i][j];
            }
            aug[i][n] = b[i];
        }

        for (size_t i = 0; i < n; ++i) {
            size_t pivot = i;
            for (size_t j = i + 1; j < n; ++j) {
                if (std::fabs(aug[j][i]) > std::fabs(aug[pivot][i])) {
                    pivot = j;
                }
            }
            std::swap(aug[i], aug[pivot]);

            if (std::fabs(aug[i][i]) < 1e-12) {
                throw std::runtime_error("Singular matrix");
            }

            for (size_t j = i + 1; j <= n; ++j) {
                aug[i][j] /= aug[i][i];
            }
            aug[i][i] = 1.0;

            for (size_t j = 0; j < n; ++j) {
                if (j != i  and  std::fabs(aug[j][i]) > 1e-12) {
                    double factor = aug[j][i];
                    for (size_t k = i; k <= n; ++k) {
                        aug[j][k] -= factor * aug[i][k];
                    }
                }
            }
        }

        std::vector<double> x(n);
        for (size_t i = 0; i < n; ++i) {
            x[i] = aug[i][n];
        }
        return x;
    }
};

}  // namespace star::common::math

#endif  // __POLYNOMIAL_H__
