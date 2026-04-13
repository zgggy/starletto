#ifndef __INTERPOLATION_H__
#define __INTERPOLATION_H__

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include "libraries/element/type/shape/vec2d.h"

namespace star::common::math {

class Interpolation {
  public:
    static auto linear(double x, const std::vector<double>& xs, const std::vector<double>& ys) -> double {
        if (xs.size() != ys.size()) {
            throw std::invalid_argument("xs and ys must have the same size");
        }
        if (xs.empty()) {
            return 0.0;
        }
        if (xs.size() == 1) {
            return ys[0];
        }
        if (x <= xs.front()) {
            return ys.front();
        }
        if (x >= xs.back()) {
            return ys.back();
        }
        auto it = std::lower_bound(xs.begin(), xs.end(), x);
        size_t i = it - xs.begin();
        double x0 = xs[i-1];
        double x1 = xs[i];
        double y0 = ys[i-1];
        double y1 = ys[i];
        double t = (x - x0) / (x1 - x0);
        return y0 + t * (y1 - y0);
    }

    static auto bilinear(double x, double y,
                         const std::vector<double>& xs,
                         const std::vector<double>& ys,
                         const std::vector<std::vector<double>>& zs) -> double {
        if (xs.size() != zs.size()  or  ys.size() != zs[0].size()) {
            throw std::invalid_argument("Size mismatch in bilinear interpolation");
        }
        if (x <= xs.front()) x = xs.front() + 1e-10;
        if (x >= xs.back()) x = xs.back() - 1e-10;
        if (y <= ys.front()) y = ys.front() + 1e-10;
        if (y >= ys.back()) y = ys.back() - 1e-10;
        auto it_x = std::lower_bound(xs.begin(), xs.end(), x);
        auto it_y = std::lower_bound(ys.begin(), ys.end(), y);
        size_t i = it_x - xs.begin();
        size_t j = it_y - ys.begin();
        double x0 = xs[i-1], x1 = xs[i];
        double y0 = ys[j-1], y1 = ys[j];
        double z00 = zs[i-1][j-1], z01 = zs[i-1][j];
        double z10 = zs[i][j-1], z11 = zs[i][j];
        double tx = (x - x0) / (x1 - x0);
        double ty = (y - y0) / (y1 - y0);
        double z0 = z00 * (1 - tx) + z10 * tx;
        double z1 = z01 * (1 - tx) + z11 * tx;
        return z0 * (1 - ty) + z1 * ty;
    }

    static auto spline(const std::vector<double>& xs, const std::vector<double>& ys) -> std::function<double(double)> {
        size_t n = xs.size();
        if (n < 2) {
            throw std::invalid_argument("Need at least 2 points for spline");
        }
        std::vector<double> h(n-1), a(n), b(n-1), c(n), d(n-1);
        for (size_t i = 0; i < n; ++i) a[i] = ys[i];
        for (size_t i = 0; i < n-1; ++i) h[i] = xs[i+1] - xs[i];
        std::vector<double> alpha(n);
        for (size_t i = 1; i < n-1; ++i) {
            alpha[i] = (3.0 / h[i]) * (a[i+1] - a[i]) - (3.0 / h[i-1]) * (a[i] - a[i-1]);
        }
        std::vector<double> l(n), mu(n), z(n);
        l[0] = 1.0;
        mu[0] = 0.0;
        z[0] = 0.0;
        for (size_t i = 1; i < n-1; ++i) {
            l[i] = 2.0 * (xs[i+1] - xs[i-1]) - h[i-1] * mu[i-1];
            mu[i] = h[i] / l[i];
            z[i] = (alpha[i] - h[i-1] * z[i-1]) / l[i];
        }
        l[n-1] = 1.0;
        z[n-1] = 0.0;
        c[n-1] = 0.0;
        for (int j = n-2; j >= 0; --j) {
            c[j] = z[j] - mu[j] * c[j+1];
            b[j] = (a[j+1] - a[j]) / h[j] - h[j] * (c[j+1] + 2.0 * c[j]) / 3.0;
            d[j] = (c[j+1] - c[j]) / (3.0 * h[j]);
        }
        return [xs, a, b, c, d](double x) -> double {
            if (x <= xs.front()) return a[0];
            if (x >= xs.back()) return a.back();
            auto it = std::upper_bound(xs.begin(), xs.end(), x);
            int i = std::max(0, static_cast<int>(it - xs.begin() - 1));
            double dx = x - xs[i];
            return a[i] + b[i] * dx + c[i] * dx * dx + d[i] * dx * dx * dx;
        };
    }
};

} // namespace star::common::math

#endif // __INTERPOLATION_H__
