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

// `rc.polygon(points, {roughness: 0, fillStyle: "solid"})`: the same outline, but the fill is the plain `M L L ...` of
// the points (renderer.js solidFillPolygon), not a closed path.
// `rc.path(d, {fill: "none"})`: only the outline, with or without the closing `Z`; `fill_d` stays empty.
rough_drawing rough_stroke_path(const std::vector<Point>& points, bool closed, int seed);

// `rc.circle(0, 0, diameter, {roughness: 0, fillStyle: "solid"})`: the fill and the outline are the same curve.
rough_drawing rough_circle(double diameter);

// `rc.path` of several `M ... L ...` runs, none closed: the outline of all of them and the fill of the first.
rough_drawing rough_open_subpaths(const std::vector<std::vector<Point>>& subpaths, int seed);

rough_drawing rough_polygon_filled_plainly(const std::vector<Point>& points, int seed);

// A number the way JavaScript's Number#toString writes it.
std::string js_number(double value);

}  // namespace mermaid::detail
