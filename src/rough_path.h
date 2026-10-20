// rough_path.h (private) - the part of rough.js that Mermaid's neo look uses for polygon shapes:
// `rc.path("M.. L.. Z", {roughness: 0, fillStyle: "solid", seed})`.
//
// With roughness 0 every offset is zero, but rough.js still draws each edge as cubic segments whose
// control points sit at a seeded random fraction of the edge, and the sequence of random numbers
// decides those fractions - so the generator is repeated step by step (renderer.js `_line`,
// `_doubleLine`, `svgPath`; generator.js `path`).
#pragma once

#include <string>
#include <vector>

#include <mermaid/geometry.h>

namespace mermaid::detail {

struct rough_drawing {
    std::string fill_d;    // solid fill: one stroke of the polygon (single `M`)
    std::string stroke_d;  // outline: every edge drawn twice
};

// The closed polygon through `points`. A seed of 0 is treated as 1 (rough.js would use Math.random()).
rough_drawing rough_polygon(const std::vector<Point>& points, int seed);

// A number the way JavaScript's Number#toString writes it.
std::string js_number(double value);

}  // namespace mermaid::detail
