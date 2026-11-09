#include "d3_shape.h"

#include <algorithm>
#include <cmath>

#include "rough_path.h"

namespace mermaid::detail {

namespace {

constexpr double kPi = 3.141592653589793;
constexpr double kHalfPi = kPi / 2;
constexpr double kTau = 2 * kPi;
constexpr double kEpsilon = 1e-12;
constexpr double kTauEpsilon = kTau - kEpsilon;

// d3-path with `digits` 3: every number is Math.round(x * 1000) / 1000 before it is written
std::string rounded(double v) { return js_number(std::floor(v * 1000 + 0.5) / 1000); }

class path_writer {
public:
    void move_to(double x, double y) {
        out_ += "M" + rounded(x) + "," + rounded(y);
        x0_ = x1_ = x;
        y0_ = y1_ = y;
        has_point_ = true;
    }
    void line_to(double x, double y) {
        out_ += "L" + rounded(x) + "," + rounded(y);
        x1_ = x;
        y1_ = y;
    }
    void close() {
        if (!has_point_) return;
        x1_ = x0_;
        y1_ = y0_;
        out_ += "Z";
    }
    // path.arc(x, y, r, a0, a1, ccw)
    void arc(double x, double y, double r, double a0, double a1, bool ccw) {
        const double dx = r * std::cos(a0), dy = r * std::sin(a0), start_x = x + dx, start_y = y + dy;
        const int cw = ccw ? 0 : 1;
        double da = ccw ? a0 - a1 : a1 - a0;
        if (!has_point_) {
            move_to(start_x, start_y);
        } else if (std::fabs(x1_ - start_x) > kEpsilon || std::fabs(y1_ - start_y) > kEpsilon) {
            line_to(start_x, start_y);
        }
        if (r == 0) return;
        if (da < 0) da = std::fmod(da, kTau) + kTau;
        const std::string radius = rounded(r) + "," + rounded(r);
        if (da > kTauEpsilon) {  // a whole circle: two half arcs
            out_ += "A" + radius + ",0,1," + std::to_string(cw) + "," + rounded(x - dx) + "," + rounded(y - dy);
            x1_ = start_x;
            y1_ = start_y;
            out_ += "A" + radius + ",0,1," + std::to_string(cw) + "," + rounded(start_x) + "," + rounded(start_y);
        } else if (da > kEpsilon) {
            x1_ = x + r * std::cos(a1);
            y1_ = y + r * std::sin(a1);
            out_ += "A" + radius + ",0," + (da >= kPi ? "1" : "0") + "," + std::to_string(cw) + "," + rounded(x1_) + "," + rounded(y1_);
        }
    }
    const std::string& str() const { return out_; }

private:
    std::string out_;
    double x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
    bool has_point_ = false;
};

}  // namespace

std::vector<pie_slice> d3_pie(const std::vector<double>& values) {
    double sum = 0;
    for (double v : values)
        if (v > 0) sum += v;
    const double k = sum != 0 ? kTau / sum : 0;
    std::vector<pie_slice> slices;
    double a0 = 0;
    for (double v : values) {
        const double a1 = a0 + (v > 0 ? v * k : 0);
        slices.push_back({a0, a1});
        a0 = a1;
    }
    return slices;
}

std::string d3_arc::path(const pie_slice& slice) const {
    double r0 = inner_, r1 = outer_;
    const double a0 = slice.start_angle - kHalfPi, a1 = slice.end_angle - kHalfPi;
    const double da = std::fabs(a1 - a0);
    const bool cw = a1 > a0;
    if (r1 < r0) std::swap(r0, r1);
    path_writer path;
    if (!(r1 > kEpsilon)) {  // a point
        path.move_to(0, 0);
    } else if (da > kTau - kEpsilon) {  // a circle or an annulus
        path.move_to(r1 * std::cos(a0), r1 * std::sin(a0));
        path.arc(0, 0, r1, a0, a1, !cw);
        if (r0 > kEpsilon) {
            path.move_to(r0 * std::cos(a1), r0 * std::sin(a1));
            path.arc(0, 0, r0, a1, a0, cw);
        }
    } else {  // a circular or annular sector (no padding, no rounded corners)
        const double x01 = r1 * std::cos(a0), y01 = r1 * std::sin(a0), x10 = r0 * std::cos(a1), y10 = r0 * std::sin(a1);
        path.move_to(x01, y01);
        if (da > kEpsilon) path.arc(0, 0, r1, a0, a1, !cw);
        if (!(r0 > kEpsilon) || !(da > kEpsilon)) path.line_to(x10, y10);
        else path.arc(0, 0, r0, a1, a0, cw);
    }
    path.close();
    return path.str();
}

std::pair<double, double> d3_arc::centroid(const pie_slice& slice) const {
    const double r = (inner_ + outer_) / 2, a = (slice.start_angle + slice.end_angle) / 2 - kHalfPi;
    return {std::cos(a) * r, std::sin(a) * r};
}

}  // namespace mermaid::detail
