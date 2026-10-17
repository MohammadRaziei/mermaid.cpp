// test_dagre.cpp - the dagre layout (mermaid::dagre::layout) against reference numbers.
//
// The expected values below were produced by running dagre-d3-es 7.0.14 (the library
// Mermaid 11.16.0 lays flowcharts out with) on exactly these inputs, with the graph set
// up the way Mermaid does it (multigraph, compound). Nothing here is guessed.
#include <cmath>
#include <string>
#include <vector>

#include <mermaid/dagre.h>
#include "utest.h"

using namespace mermaid;
using namespace mermaid::dagre;

namespace {

struct NodeExpect { const char* id; double x, y, w, h; };
struct EdgeExpect { const char* v; const char* w; const char* name; std::vector<Point> points; bool has_label; double lx, ly, lw, lh; };
struct Expect { double width, height; std::vector<NodeExpect> nodes; std::vector<EdgeExpect> edges; };

constexpr double kTol = 1e-6;

Edge edge(const char* v, const char* w, const char* name, double minlen, double weight, double width,
          double height, double offset, LabelPos pos) {
    Edge e;
    e.v = v; e.w = w; e.name = name;
    e.minlen = minlen; e.weight = weight; e.width = width; e.height = height;
    e.labeloffset = offset; e.labelpos = pos;
    return e;
}

bool near(double a, double b) { return std::fabs(a - b) <= kTol; }

// Returns "" when the layout matches, otherwise a description of the first difference.
std::string check(const Input& in, const Expect& ex) {
    const Result r = layout(in);
    if (!near(ex.width, r.width) || !near(ex.height, r.height))
        return "graph size " + std::to_string(r.width) + " x " + std::to_string(r.height) + " expected " +
               std::to_string(ex.width) + " x " + std::to_string(ex.height);
    if (ex.nodes.size() != r.nodes.size()) return "node count";
    for (const NodeExpect& n : ex.nodes) {
        const NodeResult* got = r.find_node(n.id);
        if (!got) return std::string("missing node ") + n.id;
        if (!near(n.x, got->x) || !near(n.y, got->y) || !near(n.w, got->width) || !near(n.h, got->height))
            return std::string("node ") + n.id + " at (" + std::to_string(got->x) + ", " + std::to_string(got->y) +
                   ") expected (" + std::to_string(n.x) + ", " + std::to_string(n.y) + ")";
    }
    if (ex.edges.size() != r.edges.size()) return "edge count";
    for (const EdgeExpect& e : ex.edges) {
        const std::string name = std::string("edge ") + e.v + "->" + e.w + " " + e.name;
        const EdgeResult* got = r.find_edge(e.v, e.w, e.name);
        if (!got) return "missing " + name;
        if (e.points.size() != got->points.size()) return name + ": point count";
        for (size_t i = 0; i < e.points.size(); ++i)
            if (!near(e.points[i].x, got->points[i].x) || !near(e.points[i].y, got->points[i].y))
                return name + ": point " + std::to_string(i);
        if (e.has_label != got->has_label) return name + ": label presence";
        if (e.has_label && (!near(e.lx, got->label.x) || !near(e.ly, got->label.y) ||
                            !near(e.lw, got->label_width) || !near(e.lh, got->label_height)))
            return name + ": label";
    }
    return "";
}

}  // namespace

UTEST(dagre, chain_tb) {
    Input in;
    in.options.rankdir = RankDir::TB;
    in.options.nodesep = 50.0;
    in.options.ranksep = 50.0;
    in.options.marginx = 0.0;
    in.options.marginy = 0.0;
    in.nodes.push_back({"a", 50.0, 30.0, ""});
    in.nodes.push_back({"b", 80.0, 40.0, ""});
    in.nodes.push_back({"c", 50.0, 30.0, ""});
    in.edges.push_back(edge("a", "b", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("b", "c", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    const Expect ex{80.0, 200.0,
        {
            {"a", 40.0, 15.0, 50.0, 30.0},
            {"b", 40.0, 100.0, 80.0, 40.0},
            {"c", 40.0, 185.0, 50.0, 30.0},
        },
        {
            {"a", "b", "", {{40.0, 30.0}, {40.0, 55.0}, {40.0, 80.0}}, false, 0, 0, 0, 0},
            {"b", "c", "", {{40.0, 120.0}, {40.0, 145.0}, {40.0, 170.0}}, false, 0, 0, 0, 0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, chain_bt_margins) {
    Input in;
    in.options.rankdir = RankDir::BT;
    in.options.nodesep = 50.0;
    in.options.ranksep = 50.0;
    in.options.marginx = 8.0;
    in.options.marginy = 8.0;
    in.nodes.push_back({"a", 50.0, 30.0, ""});
    in.nodes.push_back({"b", 80.0, 40.0, ""});
    in.nodes.push_back({"c", 50.0, 30.0, ""});
    in.nodes.push_back({"d", 60.0, 20.0, ""});
    in.edges.push_back(edge("a", "b", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("a", "c", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("b", "d", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("c", "d", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    const Expect ex{196.0, 206.0,
        {
            {"a", 105.5, 183.0, 50.0, 30.0},
            {"b", 48.0, 98.0, 80.0, 40.0},
            {"c", 163.0, 98.0, 50.0, 30.0},
            {"d", 105.5, 18.0, 60.0, 20.0},
        },
        {
            {"a", "b", "", {{83.9375, 168.0}, {48.0, 143.0}, {48.0, 118.0}}, false, 0, 0, 0, 0},
            {"a", "c", "", {{127.0625, 168.0}, {163.0, 143.0}, {163.0, 113.0}}, false, 0, 0, 0, 0},
            {"b", "d", "", {{48.0, 78.0}, {48.0, 53.0}, {89.07142857142857, 28.0}}, false, 0, 0, 0, 0},
            {"c", "d", "", {{163.0, 83.0}, {163.0, 53.0}, {121.92857142857143, 28.0}}, false, 0, 0, 0, 0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, fixture003_lr) {
    Input in;
    in.options.rankdir = RankDir::LR;
    in.options.nodesep = 40.0;
    in.options.ranksep = 40.0;
    in.options.marginx = 0.0;
    in.options.marginy = 0.0;
    in.nodes.push_back({"in", 152.0, 45.0, ""});
    in.nodes.push_back({"parse", 152.0, 45.0, ""});
    in.nodes.push_back({"validate", 171.0, 171.0, ""});
    in.nodes.push_back({"store", 144.0, 85.15, ""});
    in.nodes.push_back({"reject", 152.0, 45.0, ""});
    in.edges.push_back(edge("in", "parse", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("parse", "validate", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("validate", "store", "", 1.0, 1.0, 19.7, 21.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("validate", "reject", "", 1.0, 1.0, 12.0, 21.0, 10.0, LabelPos::Center));
    const Expect ex{766.7, 180.6125,
        {
            {"in", 76.0, 95.11250000000001, 152.0, 45.0},
            {"parse", 268.0, 95.11250000000001, 152.0, 45.0},
            {"validate", 469.5, 95.11250000000001, 171.0, 171.0},
            {"store", 690.7, 42.575, 144.0, 85.15},
            {"reject", 690.7, 147.65, 152.0, 45.0},
        },
        {
            {"in", "parse", "", {{152.0, 95.11250000000001}, {172.0, 95.11250000000001}, {192.0, 95.11250000000001}}, false, 0, 0, 0, 0},
            {"parse", "validate", "", {{344.0, 95.11250000000001}, {364.0, 95.11250000000001}, {384.0, 95.11250000000001}}, false, 0, 0, 0, 0},
            {"validate", "store", "", {{555.0, 56.1705299089727}, {584.85, 42.575}, {618.7, 42.575}}, true, 584.85, 42.575, 19.7, 21.0},
            {"validate", "reject", "", {{555.0, 134.0544700910273}, {584.85, 147.65}, {614.7, 147.65}}, true, 584.85, 147.65, 12.0, 21.0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, rl_labels) {
    Input in;
    in.options.rankdir = RankDir::RL;
    in.options.nodesep = 30.0;
    in.options.ranksep = 30.0;
    in.options.marginx = 0.0;
    in.options.marginy = 0.0;
    in.nodes.push_back({"s", 40.0, 40.0, ""});
    in.nodes.push_back({"t", 60.0, 30.0, ""});
    in.nodes.push_back({"u", 40.0, 40.0, ""});
    in.nodes.push_back({"v", 90.0, 50.0, ""});
    in.edges.push_back(edge("s", "t", "", 1.0, 1.0, 30.0, 14.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("s", "u", "", 1.0, 1.0, 22.0, 14.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("t", "v", "", 1.0, 1.0, 18.0, 14.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("u", "v", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    const Expect ex{298.0, 113.0,
        {
            {"s", 278.0, 56.5, 40.0, 40.0},
            {"t", 168.0, 24.0, 60.0, 30.0},
            {"u", 168.0, 89.0, 40.0, 40.0},
            {"v", 45.0, 56.5, 90.0, 50.0},
        },
        {
            {"s", "t", "", {{258.0, 43.5}, {228.0, 24.0}, {198.0, 24.0}}, true, 228.0, 7.0, 30.0, 14.0},
            {"s", "u", "", {{258.0, 69.5}, {228.0, 89.0}, {188.0, 89.0}}, true, 228.0, 106.0, 22.0, 14.0},
            {"t", "v", "", {{138.0, 24.0}, {114.0, 24.0}, {90.0, 35.30434782608695}}, true, 114.0, 24.0, 18.0, 14.0},
            {"u", "v", "", {{148.0, 89.0}, {114.0, 89.0}, {90.0, 77.69565217391305}}, false, 0, 0, 0, 0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, cycle_and_self_loops) {
    Input in;
    in.options.rankdir = RankDir::TB;
    in.options.nodesep = 50.0;
    in.options.ranksep = 50.0;
    in.options.marginx = 0.0;
    in.options.marginy = 0.0;
    in.nodes.push_back({"a", 60.0, 30.0, ""});
    in.nodes.push_back({"b", 60.0, 30.0, ""});
    in.nodes.push_back({"c", 60.0, 30.0, ""});
    in.edges.push_back(edge("a", "b", "", 1.0, 1.0, 20.0, 14.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("b", "c", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("c", "a", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("b", "b", "", 1.0, 1.0, 16.0, 14.0, 10.0, LabelPos::Right));
    const Expect ex{134.0, 204.0,
        {
            {"a", 85.5, 15.0, 60.0, 30.0},
            {"b", 30.0, 109.0, 60.0, 30.0},
            {"c", 85.5, 189.0, 60.0, 30.0},
        },
        {
            {"a", "b", "", {{67.7872340425532, 30.0}, {30.0, 62.0}, {30.0, 94.0}}, true, 30.0, 62.0, 20.0, 14.0},
            {"b", "c", "", {{30.0, 124.0}, {30.0, 149.0}, {64.6875, 174.0}}, false, 0, 0, 0, 0},
            {"c", "a", "", {{106.3125, 174.0}, {141.0, 149.0}, {141.0, 109.0}, {141.0, 62.0}, {103.2127659574468, 30.0}}, false, 0, 0, 0, 0},
            {"b", "b", "", {{60.0, 101.74193548387098}, {92.0, 94.0}, {100.0, 94.0}, {108.0, 109.0}, {100.0, 124.0}, {92.0, 124.0}, {60.0, 116.25806451612902}}, true, 126.0, 109.0, 16.0, 14.0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, multi_edges_numeric_ids) {
    Input in;
    in.options.rankdir = RankDir::TB;
    in.options.nodesep = 20.0;
    in.options.ranksep = 25.0;
    in.options.marginx = 0.0;
    in.options.marginy = 0.0;
    in.nodes.push_back({"1", 30.0, 20.0, ""});
    in.nodes.push_back({"2", 40.0, 20.0, ""});
    in.nodes.push_back({"10", 50.0, 20.0, ""});
    in.nodes.push_back({"3", 30.0, 20.0, ""});
    in.edges.push_back(edge("1", "2", "x", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("1", "2", "y", 1.0, 1.0, 12.0, 10.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("2", "10", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("10", "3", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("1", "3", "", 1.0, 1.0, 14.0, 10.0, 10.0, LabelPos::Right));
    const Expect ex{97.5, 175.0,
        {
            {"1", 38.0, 10.0, 30.0, 20.0},
            {"2", 25.0, 65.0, 40.0, 20.0},
            {"10", 25.0, 120.0, 50.0, 20.0},
            {"3", 47.5, 165.0, 30.0, 20.0},
        },
        {
            {"1", "2", "x", {{28.545454545454547, 20.0}, {12.0, 37.5}, {20.272727272727273, 55.0}}, false, 0, 0, 0, 0},
            {"1", "2", "y", {{38.0, 20.0}, {38.0, 37.5}, {29.727272727272727, 55.0}}, true, 38.0, 37.5, 12.0, 10.0},
            {"2", "10", "", {{25.0, 75.0}, {25.0, 92.5}, {25.0, 110.0}}, false, 0, 0, 0, 0},
            {"10", "3", "", {{25.0, 130.0}, {25.0, 142.5}, {37.5, 155.0}}, false, 0, 0, 0, 0},
            {"1", "3", "", {{50.90909090909091, 20.0}, {73.5, 37.5}, {73.5, 65.0}, {73.5, 92.5}, {73.5, 120.0}, {73.5, 142.5}, {59.05555555555556, 155.0}}, true, 90.5, 92.5, 14.0, 10.0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, compound_nested) {
    Input in;
    in.options.rankdir = RankDir::TB;
    in.options.nodesep = 30.0;
    in.options.ranksep = 30.0;
    in.options.marginx = 0.0;
    in.options.marginy = 0.0;
    in.nodes.push_back({"A", 0.0, 0.0, ""});
    in.nodes.push_back({"a", 40.0, 20.0, "A"});
    in.nodes.push_back({"b", 40.0, 20.0, "A"});
    in.nodes.push_back({"B", 0.0, 0.0, ""});
    in.nodes.push_back({"c", 50.0, 20.0, "B"});
    in.nodes.push_back({"C", 0.0, 0.0, "B"});
    in.nodes.push_back({"d", 50.0, 20.0, "C"});
    in.nodes.push_back({"e", 40.0, 20.0, ""});
    in.nodes.push_back({"f", 40.0, 20.0, ""});
    in.edges.push_back(edge("e", "a", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("a", "b", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("b", "c", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("c", "d", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("a", "d", "", 1.0, 1.0, 16.0, 10.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("d", "f", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    const Expect ex{170.0, 370.0,
        {
            {"A", 83.75, 100.0, 127.5, 100.0},
            {"a", 90.0, 75.0, 40.0, 20.0},
            {"b", 65.0, 125.0, 40.0, 20.0},
            {"B", 85.0, 255.0, 170.0, 130.0},
            {"c", 65.0, 215.0, 50.0, 20.0},
            {"C", 85.0, 280.0, 130.0, 50.0},
            {"d", 90.0, 280.0, 50.0, 20.0},
            {"e", 90.0, 10.0, 40.0, 20.0},
            {"f", 90.0, 360.0, 40.0, 20.0},
        },
        {
            {"e", "a", "", {{90.0, 20.0}, {90.0, 35.0}, {90.0, 50.0}, {90.0, 65.0}}, false, 0, 0, 0, 0},
            {"a", "b", "", {{80.0, 85.0}, {65.0, 100.0}, {65.0, 115.0}}, false, 0, 0, 0, 0},
            {"b", "c", "", {{65.0, 135.0}, {65.0, 150.0}, {65.0, 170.0}, {65.0, 190.0}, {65.0, 205.0}}, false, 0, 0, 0, 0},
            {"c", "d", "", {{65.0, 225.0}, {65.0, 240.0}, {65.0, 255.0}, {80.0, 270.0}}, false, 0, 0, 0, 0},
            {"a", "d", "", {{100.0, 85.0}, {115.0, 100.0}, {115.0, 125.0}, {115.0, 150.0}, {115.0, 170.0}, {115.0, 190.0}, {115.0, 215.0}, {115.0, 240.0}, {115.0, 255.0}, {100.0, 270.0}}, true, 133.0, 170.0, 16.0, 10.0},
            {"d", "f", "", {{90.0, 290.0}, {90.0, 305.0}, {90.0, 320.0}, {90.0, 335.0}, {90.0, 350.0}}, false, 0, 0, 0, 0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, random_tb_28) {
    Input in;
    in.options.rankdir = RankDir::TB;
    in.options.nodesep = 25.0;
    in.options.ranksep = 35.0;
    in.options.marginx = 5.0;
    in.options.marginy = 5.0;
    in.nodes.push_back({"n0", 21.0, 16.0, ""});
    in.nodes.push_back({"n1", 58.0, 38.0, ""});
    in.nodes.push_back({"n2", 84.0, 19.0, ""});
    in.nodes.push_back({"n3", 55.0, 35.0, ""});
    in.nodes.push_back({"n4", 62.0, 43.0, ""});
    in.nodes.push_back({"n5", 22.0, 42.0, ""});
    in.nodes.push_back({"n6", 88.0, 18.0, ""});
    in.nodes.push_back({"n7", 69.0, 32.0, ""});
    in.nodes.push_back({"n8", 48.0, 49.0, ""});
    in.nodes.push_back({"n9", 48.0, 49.0, ""});
    in.nodes.push_back({"n10", 90.0, 39.0, ""});
    in.nodes.push_back({"n11", 50.0, 33.0, ""});
    in.nodes.push_back({"n12", 40.0, 30.0, ""});
    in.nodes.push_back({"n13", 67.0, 21.0, ""});
    in.nodes.push_back({"n14", 26.0, 45.0, ""});
    in.nodes.push_back({"n15", 42.0, 16.0, ""});
    in.nodes.push_back({"n16", 52.0, 26.0, ""});
    in.nodes.push_back({"n17", 60.0, 49.0, ""});
    in.nodes.push_back({"n18", 21.0, 43.0, ""});
    in.nodes.push_back({"n19", 41.0, 27.0, ""});
    in.nodes.push_back({"n20", 87.0, 35.0, ""});
    in.nodes.push_back({"n21", 67.0, 38.0, ""});
    in.nodes.push_back({"n22", 64.0, 21.0, ""});
    in.nodes.push_back({"n23", 85.0, 42.0, ""});
    in.nodes.push_back({"n24", 52.0, 41.0, ""});
    in.nodes.push_back({"n25", 43.0, 40.0, ""});
    in.nodes.push_back({"n26", 60.0, 42.0, ""});
    in.nodes.push_back({"n27", 51.0, 23.0, ""});
    in.edges.push_back(edge("n12", "n18", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n20", "n3", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n22", "n4", "", 1.0, 1.0, 22.0, 19.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n10", "n24", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n14", "n25", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n23", "n2", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n24", "n2", "", 1.0, 1.0, 34.0, 18.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n24", "n10", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n23", "n15", "", 1.0, 1.0, 38.0, 14.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n27", "n0", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n27", "n8", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n20", "n16", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n18", "n12", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n26", "n10", "", 1.0, 3.0, 29.0, 17.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n3", "n16", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n16", "n10", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n16", "n0", "", 2.0, 1.0, 37.0, 12.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n15", "n0", "", 2.0, 1.0, 14.0, 18.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n19", "n20", "", 1.0, 3.0, 13.0, 16.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n7", "n21", "", 1.0, 1.0, 26.0, 13.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n3", "n18", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n24", "n26", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n5", "n19", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n11", "n27", "", 1.0, 1.0, 26.0, 20.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n19", "n10", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n20", "n19", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n19", "n1", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n24", "n5", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n5", "n15", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n19", "n16", "", 1.0, 3.0, 20.0, 19.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n14", "n10", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n26", "n8", "", 1.0, 1.0, 37.0, 18.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n24", "n17", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n11", "n14", "", 1.0, 1.0, 31.0, 13.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n10", "n1", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n21", "n23", "", 1.0, 1.0, 24.0, 13.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n21", "n1", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n5", "n20", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n22", "n17", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n16", "n24", "", 1.0, 1.0, 40.0, 14.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n7", "n23", "", 1.0, 1.0, 25.0, 20.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n24", "n11", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    const Expect ex{1210.0, 765.0,
        {
            {"n0", 556.25, 735.5, 21.0, 16.0},
            {"n1", 414.75, 656.0, 58.0, 38.0},
            {"n2", 151.5, 656.0, 84.0, 19.0},
            {"n3", 899.5, 29.5, 55.0, 35.0},
            {"n4", 119.0, 460.5, 62.0, 43.0},
            {"n5", 457.5, 460.5, 22.0, 42.0},
            {"n6", 996.0, 29.5, 88.0, 18.0},
            {"n7", 118.5, 271.0, 69.0, 32.0},
            {"n8", 698.0, 735.5, 48.0, 49.0},
            {"n9", 1089.0, 29.5, 48.0, 49.0},
            {"n10", 538.25, 186.5, 90.0, 39.0},
            {"n11", 518.5, 460.5, 50.0, 33.0},
            {"n12", 1014.25, 186.5, 40.0, 30.0},
            {"n13", 1171.5, 29.5, 67.0, 21.0},
            {"n14", 671.5, 560.5, 26.0, 45.0},
            {"n15", 356.5, 560.5, 42.0, 16.0},
            {"n16", 791.25, 110.5, 52.0, 26.0},
            {"n17", 356.5, 460.5, 60.0, 49.0},
            {"n18", 1014.25, 110.5, 21.0, 43.0},
            {"n19", 732.25, 560.5, 41.0, 27.0},
            {"n20", 765.75, 656.0, 87.0, 35.0},
            {"n21", 38.5, 361.5, 67.0, 38.0},
            {"n22", 145.0, 361.5, 64.0, 21.0},
            {"n23", 221.5, 460.5, 85.0, 42.0},
            {"n24", 437.5, 361.5, 52.0, 41.0},
            {"n25", 671.5, 656.0, 43.0, 40.0},
            {"n26", 599.0, 460.5, 60.0, 42.0},
            {"n27", 508.0, 656.0, 51.0, 23.0},
        },
        {
            {"n12", "n18", "", {{1018.8108108108108, 171.5}, {1025.5, 149.5}, {1020.4519230769231, 132.0}}, false, 0, 0, 0, 0},
            {"n20", "n3", "", {{809.25, 649.0489690721649}, {1056.75, 609.5}, {1056.75, 560.5}, {1056.75, 511.5}, {1056.75, 460.5}, {1056.75, 409.0}, {1056.75, 361.5}, {1056.75, 314.0}, {1056.75, 271.0}, {1056.75, 230.5}, {1056.75, 186.5}, {1056.75, 149.5}, {1056.75, 110.5}, {1056.75, 71.5}, {927.0, 36.8449920508744}}, false, 0, 0, 0, 0},
            {"n22", "n4", "", {{139.25263157894736, 372.0}, {119.0, 409.0}, {119.0, 439.0}}, true, 140.0, 409.0, 22.0, 19.0},
            {"n10", "n24", "", {{493.25, 193.61590296495956}, {260.0, 230.5}, {260.0, 271.0}, {260.0, 314.0}, {411.5, 354.5422535211268}}, false, 0, 0, 0, 0},
            {"n14", "n25", "", {{671.5, 583.0}, {671.5, 609.5}, {671.5, 636.0}}, false, 0, 0, 0, 0},
            {"n23", "n2", "", {{192.6764705882353, 481.5}, {151.5, 511.5}, {151.5, 560.5}, {151.5, 609.5}, {151.5, 646.5}}, false, 0, 0, 0, 0},
            {"n24", "n2", "", {{425.2, 382.0}, {409.0, 409.0}, {409.0, 460.5}, {409.0, 511.5}, {409.0, 560.5}, {409.0, 609.5}, {193.5, 648.4155339805825}}, true, 409.0, 511.5, 34.0, 18.0},
            {"n24", "n10", "", {{437.5, 341.0}, {437.5, 314.0}, {437.5, 271.0}, {437.5, 230.5}, {493.5994318181818, 206.0}}, false, 0, 0, 0, 0},
            {"n23", "n15", "", {{235.5, 481.5}, {255.5, 511.5}, {340.01020408163265, 552.5}}, true, 226.5, 511.5, 38.0, 14.0},
            {"n27", "n0", "", {{486.7633333333333, 667.5}, {438.75, 693.5}, {545.75, 731.7468085106383}}, false, 0, 0, 0, 0},
            {"n27", "n8", "", {{517.3533333333334, 667.5}, {538.5, 693.5}, {674.0, 729.1802507836991}}, false, 0, 0, 0, 0},
            {"n20", "n16", "", {{809.25, 643.0336538461538}, {921.75, 609.5}, {921.75, 560.5}, {921.75, 511.5}, {921.75, 460.5}, {921.75, 409.0}, {921.75, 361.5}, {921.75, 314.0}, {921.75, 271.0}, {921.75, 230.5}, {921.75, 186.5}, {921.75, 149.5}, {817.25, 118.27011494252874}}, false, 0, 0, 0, 0},
            {"n18", "n12", "", {{1008.0480769230769, 132.0}, {1003.0, 149.5}, {1009.6891891891892, 171.5}}, false, 0, 0, 0, 0},
            {"n26", "n10", "", {{599.0, 439.5}, {599.0, 409.0}, {599.0, 361.5}, {599.0, 314.0}, {599.0, 271.0}, {599.0, 230.5}, {565.1732954545455, 206.0}}, true, 599.0, 314.0, 29.0, 17.0},
            {"n3", "n16", "", {{872.0, 40.16974595842956}, {791.25, 71.5}, {791.25, 97.5}}, false, 0, 0, 0, 0},
            {"n16", "n10", "", {{765.25, 114.50790513833992}, {538.25, 149.5}, {538.25, 167.0}}, false, 0, 0, 0, 0},
            {"n16", "n0", "", {{804.75, 123.5}, {831.75, 149.5}, {831.75, 186.5}, {831.75, 230.5}, {831.75, 271.0}, {831.75, 314.0}, {831.75, 361.5}, {831.75, 409.0}, {831.75, 460.5}, {831.75, 511.5}, {831.75, 560.5}, {831.75, 609.5}, {831.75, 656.0}, {831.75, 693.5}, {566.75, 733.899274047187}}, true, 860.25, 409.0, 37.0, 12.0},
            {"n15", "n0", "", {{356.5, 568.5}, {356.5, 609.5}, {356.5, 656.0}, {356.5, 693.5}, {545.75, 733.2922403003755}}, true, 339.5, 656.0, 14.0, 18.0},
            {"n19", "n20", "", {{729.4948979591836, 574.0}, {722.25, 609.5}, {749.3790322580645, 638.5}}, true, 705.75, 609.5, 13.0, 16.0},
            {"n7", "n21", "", {{88.73255813953489, 287.0}, {38.5, 314.0}, {38.5, 342.5}}, true, 38.5, 314.0, 26.0, 13.0},
            {"n3", "n18", "", {{927.0, 39.56535947712418}, {1014.25, 71.5}, {1014.25, 89.0}}, false, 0, 0, 0, 0},
            {"n24", "n26", "", {{433.8315789473684, 382.0}, {429.0, 409.0}, {569.0, 451.4117647058824}}, false, 0, 0, 0, 0},
            {"n5", "n19", "", {{452.7647058823529, 481.5}, {446.0, 511.5}, {711.75, 556.9908296943231}}, false, 0, 0, 0, 0},
            {"n11", "n27", "", {{515.1029411764706, 477.0}, {508.0, 511.5}, {508.0, 560.5}, {508.0, 609.5}, {508.0, 644.5}}, true, 531.0, 560.5, 26.0, 20.0},
            {"n19", "n10", "", {{713.7219387755102, 547.0}, {665.0, 511.5}, {665.0, 460.5}, {665.0, 409.0}, {665.0, 361.5}, {665.0, 314.0}, {665.0, 271.0}, {665.0, 230.5}, {583.25, 202.12130177514794}}, false, 0, 0, 0, 0},
            {"n20", "n19", "", {{765.75, 638.5}, {765.75, 609.5}, {741.4795918367347, 574.0}}, false, 0, 0, 0, 0},
            {"n19", "n1", "", {{735.0051020408164, 574.0}, {742.25, 609.5}, {443.75, 651.8824427480916}}, false, 0, 0, 0, 0},
            {"n24", "n5", "", {{446.13157894736844, 382.0}, {457.5, 409.0}, {457.5, 439.5}}, false, 0, 0, 0, 0},
            {"n5", "n15", "", {{461.0, 481.5}, {466.0, 511.5}, {374.3775510204082, 552.5}}, false, 0, 0, 0, 0},
            {"n19", "n16", "", {{752.75, 556.3058455114823}, {971.75, 511.5}, {971.75, 460.5}, {971.75, 409.0}, {971.75, 361.5}, {971.75, 314.0}, {971.75, 271.0}, {971.75, 230.5}, {971.75, 186.5}, {971.75, 149.5}, {817.25, 116.11772853185596}}, true, 951.75, 314.0, 20.0, 19.0},
            {"n14", "n10", "", {{677.6989795918367, 538.0}, {685.0, 511.5}, {685.0, 460.5}, {685.0, 409.0}, {685.0, 361.5}, {685.0, 314.0}, {685.0, 271.0}, {685.0, 230.5}, {583.25, 199.9923339011925}}, false, 0, 0, 0, 0},
            {"n26", "n8", "", {{599.0, 481.5}, {599.0, 511.5}, {599.0, 560.5}, {599.0, 609.5}, {599.0, 656.0}, {599.0, 693.5}, {674.0, 725.3181818181819}}, true, 627.5, 609.5, 37.0, 18.0},
            {"n24", "n17", "", {{459.29473684210524, 382.0}, {488.0, 409.0}, {386.5, 448.7509505703422}}, false, 0, 0, 0, 0},
            {"n11", "n14", "", {{521.7352941176471, 477.0}, {528.5, 511.5}, {658.5, 556.0454545454545}}, true, 554.0, 511.5, 31.0, 13.0},
            {"n10", "n1", "", {{493.25, 194.95250800426894}, {304.0, 230.5}, {304.0, 271.0}, {304.0, 314.0}, {304.0, 361.5}, {304.0, 409.0}, {304.0, 460.5}, {304.0, 511.5}, {304.0, 560.5}, {304.0, 609.5}, {385.75, 643.823927765237}}, false, 0, 0, 0, 0},
            {"n21", "n23", "", {{27.7, 380.5}, {11.5, 409.0}, {179.0, 450.07738095238096}}, true, 33.5, 409.0, 24.0, 13.0},
            {"n21", "n1", "", {{49.3, 380.5}, {65.5, 409.0}, {65.5, 460.5}, {65.5, 511.5}, {65.5, 560.5}, {65.5, 609.5}, {385.75, 652.1388690050107}}, false, 0, 0, 0, 0},
            {"n5", "n20", "", {{468.5, 478.89344262295083}, {488.0, 511.5}, {488.0, 560.5}, {488.0, 609.5}, {722.25, 648.7173717371737}}, false, 0, 0, 0, 0},
            {"n22", "n17", "", {{153.67631578947368, 372.0}, {184.25, 409.0}, {326.5, 451.53047895500725}}, false, 0, 0, 0, 0},
            {"n16", "n24", "", {{817.25, 119.67647058823529}, {901.75, 149.5}, {901.75, 186.5}, {901.75, 230.5}, {901.75, 271.0}, {901.75, 314.0}, {463.5, 358.83979536887455}}, true, 871.75, 230.5, 40.0, 14.0},
            {"n7", "n23", "", {{153.0, 285.40291262135923}, {221.5, 314.0}, {221.5, 361.5}, {221.5, 409.0}, {221.5, 439.5}}, true, 244.0, 361.5, 25.0, 20.0},
            {"n24", "n11", "", {{463.5, 376.74691358024694}, {518.5, 409.0}, {518.5, 444.0}}, false, 0, 0, 0, 0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, random_lr_18) {
    Input in;
    in.options.rankdir = RankDir::LR;
    in.options.nodesep = 25.0;
    in.options.ranksep = 35.0;
    in.options.marginx = 5.0;
    in.options.marginy = 5.0;
    in.nodes.push_back({"n0", 65.0, 37.0, ""});
    in.nodes.push_back({"n1", 54.0, 36.0, ""});
    in.nodes.push_back({"n2", 66.0, 45.0, ""});
    in.nodes.push_back({"n3", 21.0, 35.0, ""});
    in.nodes.push_back({"n4", 55.0, 17.0, ""});
    in.nodes.push_back({"n5", 68.0, 26.0, ""});
    in.nodes.push_back({"n6", 67.0, 31.0, ""});
    in.nodes.push_back({"n7", 46.0, 16.0, ""});
    in.nodes.push_back({"n8", 74.0, 21.0, ""});
    in.nodes.push_back({"n9", 59.0, 43.0, ""});
    in.nodes.push_back({"n10", 66.0, 18.0, ""});
    in.nodes.push_back({"n11", 75.0, 22.0, ""});
    in.nodes.push_back({"n12", 78.0, 17.0, ""});
    in.nodes.push_back({"n13", 87.0, 37.0, ""});
    in.nodes.push_back({"n14", 37.0, 40.0, ""});
    in.nodes.push_back({"n15", 45.0, 23.0, ""});
    in.nodes.push_back({"n16", 75.0, 32.0, ""});
    in.nodes.push_back({"n17", 33.0, 16.0, ""});
    in.edges.push_back(edge("n17", "n0", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n17", "n2", "", 1.0, 3.0, 32.0, 10.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n10", "n7", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n15", "n13", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n9", "n5", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n11", "n0", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n14", "n0", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n4", "n12", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n10", "n16", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n3", "n4", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n14", "n17", "", 1.0, 3.0, 31.0, 20.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n11", "n14", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n12", "n11", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n0", "n1", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n0", "n11", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n13", "n2", "", 1.0, 1.0, 16.0, 19.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n17", "n4", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n12", "n1", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n11", "n4", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n15", "n10", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n8", "n0", "", 1.0, 1.0, 32.0, 13.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n8", "n2", "", 2.0, 1.0, 33.0, 17.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n5", "n9", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n13", "n14", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n16", "n9", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n15", "n16", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n8", "n10", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n9", "n16", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    const Expect ex{887.0, 504.0,
        {
            {"n0", 189.5, 384.5, 65.0, 37.0},
            {"n1", 855.0, 439.25, 54.0, 36.0},
            {"n2", 647.0, 228.75, 66.0, 45.0},
            {"n3", 530.5, 481.5, 21.0, 35.0},
            {"n4", 647.0, 461.5, 55.0, 17.0},
            {"n5", 42.0, 175.5, 68.0, 26.0},
            {"n6", 42.0, 122.0, 67.0, 31.0},
            {"n7", 305.5, 13.0, 46.0, 16.0},
            {"n8", 42.0, 71.0, 74.0, 21.0},
            {"n9", 189.5, 175.5, 59.0, 43.0},
            {"n10", 189.5, 81.0, 66.0, 18.0},
            {"n11", 305.5, 416.0, 75.0, 22.0},
            {"n12", 754.0, 439.25, 78.0, 17.0},
            {"n13", 189.5, 286.25, 87.0, 37.0},
            {"n14", 429.5, 355.0, 37.0, 40.0},
            {"n15", 42.0, 225.0, 45.0, 23.0},
            {"n16", 305.5, 210.25, 75.0, 32.0},
            {"n17", 530.5, 330.0, 33.0, 16.0},
        },
        {
            {"n17", "n0", "", {{517.3, 322.0}, {481.0, 300.0}, {429.5, 300.0}, {377.0, 300.0}, {305.5, 300.0}, {250.5, 300.0}, {202.8550295857988, 366.0}}, false, 0, 0, 0, 0},
            {"n17", "n2", "", {{547.0, 330.0}, {580.5, 330.0}, {632.2222222222222, 251.25}}, true, 580.5, 330.0, 32.0, 10.0},
            {"n10", "n7", "", {{197.5735294117647, 72.0}, {250.5, 13.0}, {282.5, 13.0}}, false, 0, 0, 0, 0},
            {"n15", "n13", "", {{55.23673469387755, 236.5}, {112.5, 286.25}, {146.0, 286.25}}, false, 0, 0, 0, 0},
            {"n9", "n5", "", {{160.0, 163.1444805194805}, {112.5, 143.25}, {70.4186046511628, 162.5}}, false, 0, 0, 0, 0},
            {"n11", "n0", "", {{268.0, 409.1818181818182}, {250.5, 406.0}, {222.0, 395.9549180327869}}, false, 0, 0, 0, 0},
            {"n14", "n0", "", {{411.0, 347.95238095238096}, {377.0, 335.0}, {305.5, 335.0}, {250.5, 335.0}, {212.2979797979798, 366.0}}, false, 0, 0, 0, 0},
            {"n4", "n12", "", {{674.5, 461.5}, {697.5, 461.5}, {732.4157303370787, 447.75}}, false, 0, 0, 0, 0},
            {"n10", "n16", "", {{192.83738601823708, 90.0}, {250.5, 245.5}, {280.5354609929078, 226.25}}, false, 0, 0, 0, 0},
            {"n3", "n4", "", {{541.0, 481.5}, {580.5, 481.5}, {619.5, 469.7706766917293}}, false, 0, 0, 0, 0},
            {"n14", "n17", "", {{448.0, 355.0}, {481.0, 355.0}, {514.66, 338.0}}, true, 481.0, 335.0, 31.0, 20.0},
            {"n11", "n14", "", {{324.6829268292683, 405.0}, {377.0, 375.0}, {411.0, 362.04761904761904}}, false, 0, 0, 0, 0},
            {"n12", "n11", "", {{733.3440860215054, 430.75}, {697.5, 416.0}, {647.0, 416.0}, {580.5, 416.0}, {530.5, 416.0}, {481.0, 416.0}, {429.5, 416.0}, {377.0, 416.0}, {343.0, 416.0}}, false, 0, 0, 0, 0},
            {"n0", "n1", "", {{197.73722627737226, 403.0}, {250.5, 521.5}, {305.5, 521.5}, {377.0, 521.5}, {429.5, 521.5}, {481.0, 521.5}, {530.5, 521.5}, {580.5, 521.5}, {647.0, 521.5}, {697.5, 521.5}, {754.0, 521.5}, {810.5, 521.5}, {845.2613981762918, 457.25}}, false, 0, 0, 0, 0},
            {"n0", "n11", "", {{204.15584415584416, 403.0}, {250.5, 461.5}, {292.2032967032967, 427.0}}, false, 0, 0, 0, 0},
            {"n13", "n2", "", {{233.0, 278.04918032786884}, {250.5, 274.75}, {305.5, 274.75}, {377.0, 274.75}, {429.5, 274.75}, {481.0, 274.75}, {530.5, 274.75}, {580.5, 274.75}, {614.4728260869565, 251.25}}, true, 429.5, 255.25, 16.0, 19.0},
            {"n17", "n4", "", {{533.541825095057, 338.0}, {580.5, 461.5}, {619.5, 461.5}}, false, 0, 0, 0, 0},
            {"n12", "n1", "", {{793.0, 439.25}, {810.5, 439.25}, {828.0, 439.25}}, false, 0, 0, 0, 0},
            {"n11", "n4", "", {{336.343137254902, 427.0}, {377.0, 441.5}, {429.5, 441.5}, {481.0, 441.5}, {530.5, 441.5}, {580.5, 441.5}, {619.5, 453.2293233082707}}, false, 0, 0, 0, 0},
            {"n15", "n10", "", {{49.96805896805897, 213.5}, {112.5, 123.25}, {173.0976331360947, 90.0}}, false, 0, 0, 0, 0},
            {"n8", "n0", "", {{44.36124401913876, 81.5}, {112.5, 384.5}, {157.0, 384.5}}, true, 112.5, 401.0, 32.0, 13.0},
            {"n8", "n2", "", {{68.91818181818182, 60.5}, {112.5, 43.5}, {189.5, 43.5}, {250.5, 43.5}, {305.5, 43.5}, {377.0, 43.5}, {429.5, 43.5}, {481.0, 43.5}, {530.5, 43.5}, {580.5, 43.5}, {638.9230769230769, 206.25}}, true, 377.0, 62.0, 33.0, 17.0},
            {"n5", "n9", "", {{76.0, 182.61347517730496}, {112.5, 190.25}, {160.0, 181.15097402597402}}, false, 0, 0, 0, 0},
            {"n13", "n14", "", {{205.91454545454545, 304.75}, {250.5, 355.0}, {305.5, 355.0}, {377.0, 355.0}, {411.0, 355.0}}, false, 0, 0, 0, 0},
            {"n16", "n9", "", {{295.38505747126436, 194.25}, {250.5, 123.25}, {214.60047846889952, 154.0}}, false, 0, 0, 0, 0},
            {"n15", "n16", "", {{64.5, 225.0}, {112.5, 225.0}, {189.5, 225.0}, {250.5, 225.0}, {268.0, 220.3068181818182}}, false, 0, 0, 0, 0},
            {"n8", "n10", "", {{79.0, 71.0}, {112.5, 71.0}, {156.5, 76.71428571428571}}, false, 0, 0, 0, 0},
            {"n9", "n16", "", {{219.0, 182.63319672131146}, {250.5, 190.25}, {268.0, 196.61363636363637}}, false, 0, 0, 0, 0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, random_compound_22) {
    Input in;
    in.options.rankdir = RankDir::TB;
    in.options.nodesep = 25.0;
    in.options.ranksep = 35.0;
    in.options.marginx = 5.0;
    in.options.marginy = 5.0;
    in.nodes.push_back({"G0", 0.0, 0.0, ""});
    in.nodes.push_back({"G1", 0.0, 0.0, ""});
    in.nodes.push_back({"G2", 0.0, 0.0, "G0"});
    in.nodes.push_back({"n0", 61.0, 16.0, ""});
    in.nodes.push_back({"n1", 86.0, 32.0, "G1"});
    in.nodes.push_back({"n2", 66.0, 19.0, ""});
    in.nodes.push_back({"n3", 76.0, 49.0, "G1"});
    in.nodes.push_back({"n4", 79.0, 38.0, ""});
    in.nodes.push_back({"n5", 32.0, 22.0, "G2"});
    in.nodes.push_back({"n6", 38.0, 37.0, ""});
    in.nodes.push_back({"n7", 81.0, 21.0, "G1"});
    in.nodes.push_back({"n8", 80.0, 20.0, ""});
    in.nodes.push_back({"n9", 76.0, 30.0, ""});
    in.nodes.push_back({"n10", 69.0, 20.0, "G1"});
    in.nodes.push_back({"n11", 78.0, 16.0, "G1"});
    in.nodes.push_back({"n12", 87.0, 25.0, ""});
    in.nodes.push_back({"n13", 67.0, 48.0, "G1"});
    in.nodes.push_back({"n14", 50.0, 49.0, "G1"});
    in.nodes.push_back({"n15", 72.0, 36.0, "G0"});
    in.nodes.push_back({"n16", 87.0, 32.0, "G1"});
    in.nodes.push_back({"n17", 63.0, 39.0, ""});
    in.nodes.push_back({"n18", 67.0, 27.0, "G1"});
    in.nodes.push_back({"n19", 65.0, 37.0, "G0"});
    in.nodes.push_back({"n20", 55.0, 41.0, "G0"});
    in.nodes.push_back({"n21", 61.0, 41.0, "G0"});
    in.edges.push_back(edge("n11", "n8", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n1", "n3", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n6", "n2", "", 1.0, 1.0, 36.0, 13.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n8", "n10", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n19", "n17", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n9", "n19", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n7", "n9", "", 1.0, 1.0, 25.0, 19.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n12", "n2", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n3", "n14", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n3", "n5", "", 1.0, 1.0, 18.0, 16.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n7", "n21", "", 1.0, 1.0, 21.0, 14.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n2", "n13", "", 1.0, 1.0, 30.0, 10.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n8", "n11", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n14", "n4", "", 2.0, 1.0, 19.0, 19.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n13", "n5", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n6", "n16", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n6", "n11", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n19", "n20", "", 1.0, 1.0, 27.0, 16.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n10", "n17", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n9", "n0", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n5", "n16", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n9", "n17", "", 1.0, 1.0, 29.0, 17.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n10", "n21", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n21", "n15", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n6", "n17", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n6", "n14", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n2", "n15", "", 1.0, 1.0, 19.0, 18.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n7", "n6", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n5", "n4", "", 1.0, 1.0, 11.0, 11.0, 10.0, LabelPos::Left));
    const Expect ex{1238.5, 648.0,
        {
            {"G0", 177.5, 499.25, 345.0, 287.5},
            {"G1", 874.5, 279.75, 513.0, 549.5},
            {"G2", 280.75, 409.0, 98.5, 72.0},
            {"n0", 416.0, 409.0, 61.0, 16.0},
            {"n1", 855.0, 101.0, 86.0, 32.0},
            {"n2", 549.5, 198.0, 66.0, 19.0},
            {"n3", 855.0, 198.0, 76.0, 49.0},
            {"n4", 412.0, 516.5, 79.0, 38.0},
            {"n5", 281.5, 409.0, 32.0, 22.0},
            {"n6", 1172.5, 101.0, 38.0, 37.0},
            {"n7", 778.0, 35.0, 81.0, 21.0},
            {"n8", 1193.5, 296.0, 80.0, 20.0},
            {"n9", 403.0, 296.0, 76.0, 30.0},
            {"n10", 838.0, 409.0, 69.0, 20.0},
            {"n11", 1052.0, 198.0, 78.0, 16.0},
            {"n12", 514.25, 35.0, 87.0, 25.0},
            {"n13", 674.0, 296.0, 67.0, 48.0},
            {"n14", 925.5, 296.0, 50.0, 49.0},
            {"n15", 128.5, 607.5, 72.0, 36.0},
            {"n16", 973.0, 516.5, 87.0, 32.0},
            {"n17", 508.0, 516.5, 63.0, 39.0},
            {"n18", 1075.0, 516.5, 67.0, 27.0},
            {"n19", 176.5, 409.0, 65.0, 37.0},
            {"n20", 62.0, 516.5, 55.0, 41.0},
            {"n21", 221.75, 516.5, 61.0, 41.0},
        },
        {
            {"n11", "n8", "", {{1055.3877551020407, 206.0}, {1072.75, 247.0}, {1168.857142857143, 286.0}}, false, 0, 0, 0, 0},
            {"n1", "n3", "", {{855.0, 117.0}, {855.0, 146.5}, {855.0, 173.5}}, false, 0, 0, 0, 0},
            {"n6", "n2", "", {{1153.5, 102.4596876319122}, {580.25, 146.5}, {555.1723300970874, 188.5}}, true, 552.25, 146.5, 36.0, 13.0},
            {"n8", "n10", "", {{1153.5, 300.7257383966245}, {838.0, 338.0}, {838.0, 355.5}, {838.0, 373.0}, {838.0, 399.0}}, false, 0, 0, 0, 0},
            {"n19", "n17", "", {{176.5, 427.5}, {176.5, 445.0}, {224.0, 470.5}, {476.5, 511.3978873239437}}, false, 0, 0, 0, 0},
            {"n9", "n19", "", {{365.0, 303.04635761589407}, {176.5, 338.0}, {176.5, 355.5}, {176.5, 373.0}, {176.5, 390.5}}, false, 0, 0, 0, 0},
            {"n7", "n9", "", {{737.5, 43.70967741935484}, {638.5, 65.0}, {638.5, 101.0}, {638.5, 146.5}, {638.5, 198.0}, {638.5, 247.0}, {441.0, 288.09341825902334}}, true, 661.0, 146.5, 25.0, 19.0},
            {"n12", "n2", "", {{514.25, 47.5}, {514.25, 65.0}, {514.25, 101.0}, {514.25, 146.5}, {542.997572815534, 188.5}}, false, 0, 0, 0, 0},
            {"n3", "n14", "", {{893.0, 220.10089020771514}, {939.25, 247.0}, {932.375, 271.5}}, false, 0, 0, 0, 0},
            {"n3", "n5", "", {{817.0, 212.89600000000002}, {730.0, 247.0}, {730.0, 296.0}, {730.0, 338.0}, {730.0, 355.5}, {730.0, 373.0}, {297.5, 407.7157190635451}}, true, 749.0, 296.0, 18.0, 16.0},
            {"n7", "n21", "", {{779.05, 45.5}, {781.0, 65.0}, {781.0, 101.0}, {781.0, 146.5}, {781.0, 198.0}, {781.0, 247.0}, {781.0, 296.0}, {781.0, 338.0}, {781.0, 355.5}, {781.0, 373.0}, {781.0, 409.0}, {781.0, 445.0}, {781.0, 470.5}, {252.25, 513.9912829682611}}, true, 760.5, 247.0, 21.0, 14.0},
            {"n2", "n13", "", {{573.6377551020408, 207.5}, {674.0, 247.0}, {674.0, 272.0}}, true, 674.0, 247.0, 30.0, 10.0},
            {"n8", "n11", "", {{1160.3877551020407, 286.0}, {1031.25, 247.0}, {1048.6122448979593, 206.0}}, false, 0, 0, 0, 0},
            {"n14", "n4", "", {{925.5, 320.5}, {925.5, 338.0}, {925.5, 355.5}, {925.5, 373.0}, {925.5, 409.0}, {925.5, 445.0}, {925.5, 470.5}, {451.5, 512.9615384615385}}, true, 906.0, 409.0, 19.0, 19.0},
            {"n13", "n5", "", {{674.0, 320.0}, {674.0, 338.0}, {674.0, 355.5}, {674.0, 373.0}, {297.5, 407.5324840764331}}, false, 0, 0, 0, 0},
            {"n6", "n16", "", {{1153.5, 105.33333333333333}, {973.0, 146.5}, {973.0, 198.0}, {973.0, 247.0}, {973.0, 296.0}, {973.0, 338.0}, {973.0, 355.5}, {973.0, 373.0}, {973.0, 409.0}, {973.0, 445.0}, {973.0, 470.5}, {973.0, 500.5}}, false, 0, 0, 0, 0},
            {"n6", "n11", "", {{1153.5, 108.17427385892117}, {1052.0, 146.5}, {1052.0, 190.0}}, false, 0, 0, 0, 0},
            {"n19", "n20", "", {{144.0, 419.2183406113537}, {62.0, 445.0}, {62.0, 470.5}, {62.0, 496.0}}, true, 38.5, 470.5, 27.0, 16.0},
            {"n10", "n17", "", {{868.9027777777778, 419.0}, {949.25, 445.0}, {949.25, 470.5}, {539.5, 513.2161473087818}}, false, 0, 0, 0, 0},
            {"n9", "n0", "", {{407.64285714285717, 311.0}, {416.0, 338.0}, {416.0, 355.5}, {416.0, 373.0}, {416.0, 401.0}}, false, 0, 0, 0, 0},
            {"n5", "n16", "", {{284.55555555555554, 420.0}, {291.5, 445.0}, {291.5, 470.5}, {929.5, 513.563829787234}}, false, 0, 0, 0, 0},
            {"n9", "n17", "", {{441.0, 307.64963503649636}, {540.0, 338.0}, {540.0, 355.5}, {540.0, 373.0}, {540.0, 409.0}, {540.0, 445.0}, {540.0, 470.5}, {521.5652173913044, 497.0}}, true, 564.5, 409.0, 29.0, 17.0},
            {"n10", "n21", "", {{803.5, 417.9675090252708}, {699.5, 445.0}, {699.5, 470.5}, {252.25, 513.5633176347462}}, false, 0, 0, 0, 0},
            {"n21", "n15", "", {{221.75, 537.0}, {221.75, 554.5}, {221.75, 572.0}, {164.5, 593.7949061662198}}, false, 0, 0, 0, 0},
            {"n6", "n17", "", {{1153.5, 102.24209770114942}, {476.5, 146.5}, {476.5, 198.0}, {476.5, 247.0}, {476.5, 296.0}, {476.5, 338.0}, {476.5, 355.5}, {476.5, 373.0}, {476.5, 409.0}, {476.5, 445.0}, {476.5, 470.5}, {494.6467391304348, 497.0}}, false, 0, 0, 0, 0},
            {"n6", "n14", "", {{1153.5, 104.36381322957199}, {915.5, 146.5}, {915.5, 198.0}, {915.5, 247.0}, {920.5, 271.5}}, false, 0, 0, 0, 0},
            {"n2", "n15", "", {{516.5, 201.696}, {112.0, 247.0}, {112.0, 296.0}, {112.0, 338.0}, {112.0, 355.5}, {112.0, 373.0}, {112.0, 409.0}, {112.0, 445.0}, {112.0, 470.5}, {112.0, 516.5}, {112.0, 554.5}, {112.0, 572.0}, {120.13380281690141, 589.5}}, true, 112.0, 409.0, 19.0, 18.0},
            {"n7", "n6", "", {{818.5, 41.85472496473907}, {955.25, 65.0}, {1153.5, 97.85155350978135}}, false, 0, 0, 0, 0},
            {"n5", "n4", "", {{278.44444444444446, 420.0}, {271.5, 445.0}, {204.0, 470.5}, {372.5, 507.7644230769231}}, true, 188.5, 470.5, 11.0, 11.0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

UTEST(dagre, random_compound_lr_16) {
    Input in;
    in.options.rankdir = RankDir::LR;
    in.options.nodesep = 25.0;
    in.options.ranksep = 35.0;
    in.options.marginx = 5.0;
    in.options.marginy = 5.0;
    in.nodes.push_back({"G0", 0.0, 0.0, ""});
    in.nodes.push_back({"G1", 0.0, 0.0, ""});
    in.nodes.push_back({"G2", 0.0, 0.0, "G0"});
    in.nodes.push_back({"n0", 56.0, 47.0, "G0"});
    in.nodes.push_back({"n1", 41.0, 41.0, "G0"});
    in.nodes.push_back({"n2", 21.0, 22.0, ""});
    in.nodes.push_back({"n3", 83.0, 42.0, ""});
    in.nodes.push_back({"n4", 28.0, 32.0, ""});
    in.nodes.push_back({"n5", 39.0, 22.0, "G0"});
    in.nodes.push_back({"n6", 51.0, 22.0, "G2"});
    in.nodes.push_back({"n7", 60.0, 26.0, "G0"});
    in.nodes.push_back({"n8", 85.0, 23.0, "G2"});
    in.nodes.push_back({"n9", 66.0, 39.0, ""});
    in.nodes.push_back({"n10", 86.0, 50.0, "G1"});
    in.nodes.push_back({"n11", 47.0, 27.0, "G2"});
    in.nodes.push_back({"n12", 73.0, 27.0, "G2"});
    in.nodes.push_back({"n13", 86.0, 17.0, ""});
    in.nodes.push_back({"n14", 84.0, 47.0, "G1"});
    in.nodes.push_back({"n15", 34.0, 21.0, "G1"});
    in.edges.push_back(edge("n15", "n14", "", 1.0, 1.0, 23.0, 11.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n11", "n5", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n3", "n6", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n7", "n11", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n13", "n15", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n1", "n8", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n6", "n6", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n12", "n0", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n4", "n2", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n2", "n4", "", 1.0, 3.0, 36.0, 13.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n2", "n0", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n14", "n14", "", 2.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n3", "n10", "", 1.0, 3.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n6", "n8", "", 1.0, 1.0, 31.0, 10.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n13", "n1", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Right));
    in.edges.push_back(edge("n9", "n6", "", 2.0, 1.0, 39.0, 14.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n9", "n7", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n1", "n7", "", 2.0, 1.0, 25.0, 14.0, 10.0, LabelPos::Left));
    in.edges.push_back(edge("n1", "n13", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    in.edges.push_back(edge("n0", "n8", "", 1.0, 1.0, 0.0, 0.0, 10.0, LabelPos::Center));
    const Expect ex{691.0, 587.5,
        {
            {"G0", 345.5, 412.25, 681.0, 340.5},
            {"G1", 359.75, 82.0, 280.5, 154.0},
            {"G2", 452.75, 438.25, 431.5, 208.5},
            {"n0", 440.5, 288.0, 56.0, 47.0},
            {"n1", 43.0, 502.25, 41.0, 41.0},
            {"n2", 297.5, 195.75, 21.0, 22.0},
            {"n3", 141.5, 111.5, 83.0, 42.0},
            {"n4", 608.5, 189.0, 28.0, 32.0},
            {"n5", 608.5, 288.0, 39.0, 22.0},
            {"n6", 440.5, 450.25, 51.0, 22.0},
            {"n7", 297.5, 290.25, 60.0, 26.0},
            {"n8", 608.5, 435.75, 85.0, 23.0},
            {"n9", 141.5, 188.5, 66.0, 39.0},
            {"n10", 297.5, 111.5, 86.0, 50.0},
            {"n11", 440.5, 380.75, 47.0, 27.0},
            {"n12", 297.5, 400.75, 73.0, 27.0},
            {"n13", 141.5, 51.0, 86.0, 17.0},
            {"n14", 440.5, 51.0, 84.0, 47.0},
            {"n15", 297.5, 51.0, 34.0, 21.0},
        },
        {
            {"n15", "n14", "", {{314.5, 51.0}, {369.5, 51.0}, {398.5, 51.0}}, true, 369.5, 51.0, 23.0, 11.0},
            {"n11", "n5", "", {{464.0, 380.75}, {500.0, 380.75}, {533.0, 380.75}, {599.5458221024259, 299.0}}, false, 0, 0, 0, 0},
            {"n3", "n6", "", {{147.0601750547046, 132.5}, {202.0, 340.0}, {219.5, 340.0}, {237.0, 436.75}, {297.5, 436.75}, {369.5, 436.75}, {415.0, 445.40140845070425}}, false, 0, 0, 0, 0},
            {"n7", "n11", "", {{307.8425414364641, 303.25}, {369.5, 380.75}, {417.0, 380.75}}, false, 0, 0, 0, 0},
            {"n13", "n15", "", {{184.5, 51.0}, {202.0, 51.0}, {219.5, 51.0}, {237.0, 51.0}, {280.5, 51.0}}, false, 0, 0, 0, 0},
            {"n1", "n8", "", {{63.5, 517.7598684210526}, {81.0, 531.0}, {141.5, 531.0}, {202.0, 531.0}, {219.5, 531.0}, {237.0, 515.5}, {297.5, 515.5}, {369.5, 515.5}, {440.5, 515.5}, {500.0, 515.5}, {533.0, 515.5}, {597.6128526645768, 447.25}}, false, 0, 0, 0, 0},
            {"n6", "n6", "", {{431.3032786885246, 461.25}, {415.0, 480.75}, {415.0, 485.625}, {440.5, 490.5}, {466.0, 485.625}, {466.0, 480.75}, {449.6967213114754, 461.25}}, true, 440.5, 485.5, 0.0, 0.0},
            {"n12", "n0", "", {{334.0, 400.75}, {369.5, 400.75}, {425.70177383592016, 311.5}}, false, 0, 0, 0, 0},
            {"n4", "n2", "", {{594.5, 187.1456953642384}, {533.0, 179.0}, {500.0, 179.0}, {440.5, 179.0}, {369.5, 179.0}, {308.0, 193.30729166666666}}, false, 0, 0, 0, 0},
            {"n2", "n4", "", {{308.0, 196.22395833333334}, {369.5, 199.0}, {440.5, 199.0}, {500.0, 199.0}, {533.0, 199.0}, {594.5, 190.8543046357616}}, true, 440.5, 215.5, 36.0, 13.0},
            {"n2", "n0", "", {{306.0853658536585, 206.75}, {369.5, 288.0}, {412.5, 288.0}}, false, 0, 0, 0, 0},
            {"n14", "n14", "", {{421.7, 74.5}, {398.5, 103.5}, {398.5, 110.75}, {440.5, 118.0}, {482.5, 110.75}, {482.5, 103.5}, {459.3, 74.5}}, true, 440.5, 113.0, 0.0, 0.0},
            {"n3", "n10", "", {{183.0, 111.5}, {202.0, 111.5}, {219.5, 111.5}, {237.0, 111.5}, {254.5, 111.5}}, false, 0, 0, 0, 0},
            {"n6", "n8", "", {{466.0, 450.25}, {500.0, 450.25}, {533.0, 450.25}, {566.0, 443.9122516556291}}, true, 533.0, 450.25, 31.0, 10.0},
            {"n13", "n1", "", {{139.8153153153153, 59.5}, {81.0, 356.25}, {48.33561643835616, 481.75}}, false, 0, 0, 0, 0},
            {"n9", "n6", "", {{145.78610354223434, 208.0}, {202.0, 463.75}, {219.5, 463.75}, {237.0, 463.75}, {297.5, 463.75}, {369.5, 463.75}, {415.0, 455.09859154929575}}, true, 297.5, 463.75, 39.0, 14.0},
            {"n9", "n7", "", {{148.37900874635568, 208.0}, {202.0, 360.0}, {219.5, 360.0}, {237.0, 290.25}, {267.5, 290.25}}, false, 0, 0, 0, 0},
            {"n1", "n7", "", {{63.5, 506.9703947368421}, {81.0, 511.0}, {141.5, 511.0}, {202.0, 511.0}, {219.5, 511.0}, {237.0, 562.5}, {294.6111111111111, 303.25}}, true, 141.5, 494.0, 25.0, 14.0},
            {"n1", "n13", "", {{49.182539682539684, 481.75}, {81.0, 376.25}, {139.91890853189855, 59.5}}, false, 0, 0, 0, 0},
            {"n0", "n8", "", {{459.71993127147766, 311.5}, {500.0, 360.75}, {533.0, 360.75}, {596.9233333333333, 424.25}}, false, 0, 0, 0, 0},
        }};
    ASSERT_STREQ("", check(in, ex).c_str());
}

