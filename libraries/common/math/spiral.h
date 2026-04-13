#ifndef __SPIRAL_H__
#define __SPIRAL_H__

#include <cmath>
#include <vector>

namespace star::common::math {

constexpr double kPI = 3.14159265358979323846;
constexpr double kTwoPI = 2.0 * kPI;

class FresnelIntegrals {
public:
    static void compute(double t, double& c, double& s) {
        if (std::fabs(t) < 1e-9) {
            c = 0.0;
            s = 0.0;
            return;
        }

        if (std::fabs(t) > 4.5) {
            c = 0.5;
            s = 0.5;
            if (t < 0.0) {
                c = -c;
                s = -s;
            }
            return;
        }

        compute_series(t, c, s);
    }

private:
    static void compute_series(double t, double& c, double& s) {
        double x = kPI * t * t / 2.0;
        double sign = t < 0.0 ? -1.0 : 1.0;
        double t_abs = std::fabs(t);

        if (t_abs < 1.0) {
            compute_taylor_series(x, t, c, s);
        } else {
            compute_continued_fraction(x, t, c, s);
        }

        c *= sign;
        s *= sign;
    }

    static void compute_taylor_series(double x, double t, double& c, double& s) {
        c = 0.0;
        s = 0.0;

        double term_c = t;
        double term_s = x * t / 3.0;
        c = term_c;
        s = term_s;

        for (int n = 1; n < 50; ++n) {
            double k = 2.0 * n;
            double fact = k * (k + 1.0);
            term_c *= -x * x / fact;
            c += term_c;

            k = 2.0 * n + 1.0;
            fact = k * (k + 1.0);
            term_s *= -x * x / fact;
            s += term_s;

            if (std::fabs(term_c) < 1e-12  and  std::fabs(term_s) < 1e-12) {
                break;
            }
        }
    }

    static void compute_continued_fraction(double x, double t, double& c, double& s) {
        double sign = t < 0.0 ? -1.0 : 1.0;
        double t_abs = std::fabs(t);

        c = 0.5 * sign;
        s = 0.5 * sign;

        double f, g;
        compute_asymptotic(x, f, g);

        c -= sign * g * std::sin(x) / (kPI * t_abs);
        s -= sign * f * std::cos(x) / (kPI * t_abs);
    }

    static void compute_asymptotic(double x, double& f, double& g) {
        double t = 1.0 / x;
        f = 1.0;
        g = t;

        double term = 1.0;
        for (int n = 1; n < 20; ++n) {
            term *= -(2.0 * n - 1.0) * (2.0 * n) * t * t;
            f += term;

            term *= (2.0 * n) * (2.0 * n + 1.0) * t * t;
            g += term;

            if (std::fabs(term) < 1e-10) {
                break;
            }
        }
    }
};

class SpiralSegment {
public:
    double x_start = 0.0;
    double y_start = 0.0;
    double theta_start = 0.0;
    double kappa_start = 0.0;
    double dkappa = 0.0;
    double length = 0.0;

    SpiralSegment() = default;

    SpiralSegment(double x, double y, double theta, double kappa, double dk, double len)
        : x_start(x), y_start(y), theta_start(theta), kappa_start(kappa), dkappa(dk), length(len) {}

    void get_point(double s, double& x, double& y, double& theta, double& kappa) const {
        if (s < 0.0) {
            s = 0.0;
        }
        if (s > length) {
            s = length;
        }

        kappa = kappa_start + dkappa * s;
        theta = theta_start + kappa_start * s + 0.5 * dkappa * s * s;

        if (std::fabs(dkappa) < 1e-9) {
            if (std::fabs(kappa_start) < 1e-9) {
                x = x_start + s * std::cos(theta_start);
                y = y_start + s * std::sin(theta_start);
            } else {
                double r = 1.0 / kappa_start;
                x = x_start + r * (std::sin(theta) - std::sin(theta_start));
                y = y_start - r * (std::cos(theta) - std::cos(theta_start));
            }
            return;
        }

        double a = std::sqrt(std::fabs(dkappa) / kPI);
        double t0 = kappa_start / (kPI * a * a);
        double t1 = (kappa_start + dkappa * s) / (kPI * a * a);

        double c0, s0, c1, s1;
        FresnelIntegrals::compute(t0, c0, s0);
        FresnelIntegrals::compute(t1, c1, s1);

        double delta_c = c1 - c0;
        double delta_s = s1 - s0;

        if (dkappa > 0) {
            x = x_start + (delta_c * std::cos(theta_start) + delta_s * std::sin(theta_start)) / a;
            y = y_start + (-delta_s * std::cos(theta_start) + delta_c * std::sin(theta_start)) / a;
        } else {
            x = x_start + (delta_c * std::cos(theta_start) - delta_s * std::sin(theta_start)) / a;
            y = y_start + (delta_s * std::cos(theta_start) + delta_c * std::sin(theta_start)) / a;
        }
    }

    auto sample_x(double step) const -> std::vector<double> {
        std::vector<double> result;
        for (double s = 0.0; s <= length + 1e-9; s += step) {
            double x, y, t, k;
            get_point(s, x, y, t, k);
            result.push_back(x);
        }
        return result;
    }

    auto sample_y(double step) const -> std::vector<double> {
        std::vector<double> result;
        for (double s = 0.0; s <= length + 1e-9; s += step) {
            double x, y, t, k;
            get_point(s, x, y, t, k);
            result.push_back(y);
        }
        return result;
    }

    auto sample_theta(double step) const -> std::vector<double> {
        std::vector<double> result;
        for (double s = 0.0; s <= length + 1e-9; s += step) {
            double x, y, t, k;
            get_point(s, x, y, t, k);
            result.push_back(t);
        }
        return result;
    }

    auto sample_kappa(double step) const -> std::vector<double> {
        std::vector<double> result;
        for (double s = 0.0; s <= length + 1e-9; s += step) {
            double x, y, t, k;
            get_point(s, x, y, t, k);
            result.push_back(k);
        }
        return result;
    }
};

} // namespace star::common::math

#endif // __SPIRAL_H__
