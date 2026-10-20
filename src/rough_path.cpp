#include "rough_path.h"

#include <charconv>
#include <cmath>
#include <cstdint>

namespace mermaid::detail {
namespace {

// roughjs/bin/math.js Random
class rough_random {
public:
    explicit rough_random(int seed) : seed_(seed) {}
    double next() {
        seed_ = static_cast<int32_t>(static_cast<uint32_t>(48271) * static_cast<uint32_t>(seed_));
        return static_cast<double>(seed_ & 0x7FFFFFFF) / 2147483648.0;
    }

private:
    int32_t seed_;
};

struct rough_op {
    bool curve = false;  // false: move (data[0..1]), true: bcurveTo (data[0..5])
    double data[6] = {0, 0, 0, 0, 0, 0};
};

// The options Mermaid passes for non-hand-drawn shapes (roughness 0, solid) on top of rough.js's defaults.
constexpr double kRoughness = 0;
constexpr double kMaxRandomnessOffset = 2;
constexpr double kBowing = 1;

double offset_opt(double x, double gain, rough_random& random) { return kRoughness * gain * ((random.next() * (x - -x)) + -x); }

// renderer.js _line with `move` set.
std::vector<rough_op> line(double x1, double y1, double x2, double y2, bool overlay, rough_random& random) {
    const double length_sq = std::pow(x1 - x2, 2) + std::pow(y1 - y2, 2);
    const double length = std::sqrt(length_sq);
    double gain = 1;
    if (length < 200) gain = 1;
    else if (length > 500) gain = 0.4;
    else gain = -0.0016668 * length + 1.233334;
    double offset = kMaxRandomnessOffset;
    if (offset * offset * 100 > length_sq) offset = length / 10;
    const double half = offset / 2;
    const double diverge = 0.2 + random.next() * 0.2;
    double mid_x = kBowing * kMaxRandomnessOffset * (y2 - y1) / 200;
    double mid_y = kBowing * kMaxRandomnessOffset * (x1 - x2) / 200;
    mid_x = offset_opt(mid_x, gain, random);
    mid_y = offset_opt(mid_y, gain, random);

    const double spread = overlay ? half : offset;  // randomHalf / randomFull
    rough_op move, curve;
    curve.curve = true;
    // The calls are made one by one in rough.js's order: the random stream is shared.
    const double mx = offset_opt(spread, gain, random);
    const double my = offset_opt(spread, gain, random);
    move.data[0] = x1 + mx;
    move.data[1] = y1 + my;
    const double r0 = offset_opt(spread, gain, random);
    const double r1 = offset_opt(spread, gain, random);
    const double r2 = offset_opt(spread, gain, random);
    const double r3 = offset_opt(spread, gain, random);
    const double r4 = offset_opt(spread, gain, random);
    const double r5 = offset_opt(spread, gain, random);
    curve.data[0] = mid_x + x1 + (x2 - x1) * diverge + r0;
    curve.data[1] = mid_y + y1 + (y2 - y1) * diverge + r1;
    curve.data[2] = mid_x + x1 + 2 * (x2 - x1) * diverge + r2;
    curve.data[3] = mid_y + y1 + 2 * (y2 - y1) * diverge + r3;
    curve.data[4] = x2 + r4;
    curve.data[5] = y2 + r5;
    return {move, curve};
}

// renderer.js _doubleLine
void double_line(std::vector<rough_op>& ops, const Point& a, const Point& b, bool single, rough_random& random) {
    for (const rough_op& op : line(a.x, a.y, b.x, b.y, false, random)) ops.push_back(op);
    if (single) return;
    for (const rough_op& op : line(a.x, a.y, b.x, b.y, true, random)) ops.push_back(op);
}

// renderer.js svgPath for a path made of M, L... and Z.
std::vector<rough_op> svg_path(const std::vector<Point>& points, bool single, rough_random& random) {
    std::vector<rough_op> ops;
    if (points.empty()) return ops;
    Point current = points.front();
    for (size_t i = 1; i < points.size(); ++i) {
        double_line(ops, current, points[i], single, random);
        current = points[i];
    }
    double_line(ops, current, points.front(), single, random);  // Z
    return ops;
}

// generator.js opsToPath
std::string ops_to_path(const std::vector<rough_op>& ops, bool merged) {
    std::string path;
    bool first = true;
    for (const rough_op& op : ops) {
        if (!op.curve) {
            if (merged && !first) continue;
            path += "M" + js_number(op.data[0]) + " " + js_number(op.data[1]) + " ";
        } else {
            path += "C" + js_number(op.data[0]) + " " + js_number(op.data[1]) + ", " + js_number(op.data[2]) + " " +
                    js_number(op.data[3]) + ", " + js_number(op.data[4]) + " " + js_number(op.data[5]) + " ";
        }
        first = false;
    }
    if (!path.empty()) path.pop_back();
    return path;
}

}  // namespace

std::string js_number(double v) {
    if (v == 0) return "0";  // also -0
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v < 0 ? "-Infinity" : "Infinity";
    char buffer[64];
    const auto res = std::to_chars(buffer, buffer + sizeof buffer, std::fabs(v), std::chars_format::scientific);
    const std::string sci(buffer, res.ptr);  // d[.ddd]e[+-]XX
    const size_t e_at = sci.find('e');
    std::string digits;
    for (size_t i = 0; i < e_at; ++i)
        if (sci[i] != '.') digits += sci[i];
    const int exponent = std::stoi(sci.substr(e_at + 1));
    const int k = static_cast<int>(digits.size());
    const int n = exponent + 1;
    std::string out;
    if (k <= n && n <= 21) out = digits + std::string(static_cast<size_t>(n - k), '0');
    else if (0 < n && n <= 21) out = digits.substr(0, static_cast<size_t>(n)) + "." + digits.substr(static_cast<size_t>(n));
    else if (-6 < n && n <= 0) out = "0." + std::string(static_cast<size_t>(-n), '0') + digits;
    else {
        const int e = n - 1;
        out = digits.substr(0, 1) + (k > 1 ? "." + digits.substr(1) : "") + "e" + (e < 0 ? "-" : "+") + std::to_string(std::abs(e));
    }
    return (v < 0 ? "-" : "") + out;
}

rough_drawing rough_polygon(const std::vector<Point>& points, int seed) {
    rough_random random(seed == 0 ? 1 : seed);
    // generator.js path(): the outline is generated first, and the solid fill goes on with the same
    // random stream (the options copy shares the randomizer).
    const std::vector<rough_op> stroke = svg_path(points, false, random);
    const std::vector<rough_op> fill = svg_path(points, true, random);
    return {ops_to_path(fill, true), ops_to_path(stroke, false)};
}

}  // namespace mermaid::detail
