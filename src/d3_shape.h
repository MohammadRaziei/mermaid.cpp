// d3_shape.h (private) - the two generators Mermaid's pie chart takes from d3-shape, with d3-path's output:
// `pie()` (angles of values, in their order) and `arc()` (the path of a slice, numbers rounded to three digits).
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace mermaid::detail {

struct pie_slice {
    double start_angle = 0;
    double end_angle = 0;
};

// d3.pie().value(v).sort(null): the whole circle shared out by the values (a value of 0 or less gets no angle).
std::vector<pie_slice> d3_pie(const std::vector<double>& values);

class d3_arc {
public:
    d3_arc(double inner_radius, double outer_radius) : inner_(inner_radius), outer_(outer_radius) {}

    // arc(slice): the `d` of the slice, e.g. `M0,-185A185,185,0,1,1,-178.157,-49.85L0,0Z`.
    std::string path(const pie_slice& slice) const;
    // arc.centroid(slice): the middle of the slice between the two radii, not rounded.
    std::pair<double, double> centroid(const pie_slice& slice) const;

private:
    double inner_;
    double outer_;
};

}  // namespace mermaid::detail
