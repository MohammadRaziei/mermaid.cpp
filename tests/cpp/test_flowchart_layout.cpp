#include <cmath>
#include <fstream>
#include <sstream>

#include <mermaid/approx_text_measurer.h>
#include <mermaid/flowchart_layout.h>
#include <mermaid/flowchart_parser.h>
#include "utest.h"

using namespace mermaid;
using namespace mermaid::flowchart;

namespace {

Layout lay(const std::string& src, LayoutOptions opt = {}) {
    static ApproxTextMeasurer measurer;
    return LayoutEngine(measurer, opt).layout(Parser().parse(src));
}

std::string read_fixture(const std::string& name) {
    std::ifstream in(std::string(MERMAID_TEST_DATA_DIR) + "/merman/" + name + ".mmd", std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool overlaps(const Rect& a, const Rect& b) {
    const double eps = 1e-6;
    return a.x < b.right() - eps && b.x < a.right() - eps && a.y < b.bottom() - eps &&
           b.y < a.bottom() - eps;
}

}  // namespace

// Numbers below come from merman's layout goldens (tests/data/merman/*.layout.golden.json).
UTEST(flowchart_layout, basic_matches_merman_golden_nodes) {
    Layout l = lay(read_fixture("basic"));
    ASSERT_EQ(4u, l.nodes.size());
    const NodeLayout* a = l.find_node("A");
    const NodeLayout* b = l.find_node("B");
    const NodeLayout* c = l.find_node("C");
    const NodeLayout* d = l.find_node("D");
    ASSERT_TRUE(a && b && c && d);
    ASSERT_NEAR(184.0, a->center.x, 1e-6);
    ASSERT_NEAR(34.5, a->center.y, 1e-6);
    ASSERT_NEAR(152.0, a->size.width, 1e-6);
    ASSERT_NEAR(45.0, a->size.height, 1e-6);
    ASSERT_NEAR(184.0, b->center.x, 1e-6);
    ASSERT_NEAR(182.5, b->center.y, 1e-6);
    ASSERT_NEAR(171.0, b->size.width, 1e-6);
    ASSERT_NEAR(171.0, b->size.height, 1e-6);
    ASSERT_NEAR(88.0, c->center.x, 1e-6);
    ASSERT_NEAR(391.5, c->center.y, 1e-6);
    ASSERT_NEAR(280.0, d->center.x, 1e-6);
    ASSERT_NEAR(391.5, d->center.y, 1e-6);
}

UTEST(flowchart_layout, basic_matches_merman_golden_bounds_and_edge) {
    Layout l = lay(read_fixture("basic"));
    ASSERT_NEAR(12.0, l.bounds.x, 1e-6);
    ASSERT_NEAR(12.0, l.bounds.y, 1e-6);
    ASSERT_NEAR(356.0, l.bounds.right(), 1e-6);
    ASSERT_NEAR(414.0, l.bounds.bottom(), 1e-6);
    ASSERT_NEAR(368.0, l.size.width, 1e-6);
    ASSERT_NEAR(426.0, l.size.height, 1e-6);
    const EdgeLayout& e = l.edges[0];
    ASSERT_STREQ("L_A_B_0", e.id.c_str());
    ASSERT_EQ(2u, e.points.size());
    ASSERT_NEAR(184.0, e.points[0].x, 1e-6);
    ASSERT_NEAR(57.0, e.points[0].y, 1e-6);
    ASSERT_NEAR(184.0, e.points[1].x, 1e-6);
    ASSERT_NEAR(97.0, e.points[1].y, 1e-6);
}

UTEST(flowchart_layout, class_style_matches_merman_golden) {
    Layout l = lay(read_fixture("class_style"));
    const NodeLayout* a = l.find_node("A");
    const NodeLayout* b = l.find_node("B");
    ASSERT_NEAR(88.0, a->center.x, 1e-6);
    ASSERT_NEAR(34.5, a->center.y, 1e-6);
    ASSERT_NEAR(88.0, b->center.x, 1e-6);
    ASSERT_NEAR(119.5, b->center.y, 1e-6);
}

UTEST(flowchart_layout, labeled_edges_get_a_label_box_between_the_ranks) {
    Layout l = lay(read_fixture("basic"));
    ASSERT_TRUE(!l.edges[0].has_label);
    const EdgeLayout& yes = l.edges[1];
    ASSERT_TRUE(yes.has_label);
    const NodeLayout* b = l.find_node("B");
    const NodeLayout* c = l.find_node("C");
    ASSERT_TRUE(yes.label.center().y > b->rect().bottom());
    ASSERT_TRUE(yes.label.center().y < c->rect().y);
    ASSERT_NEAR(21.0, yes.label.height, 1e-6);
}

UTEST(flowchart_layout, direction_changes_the_flow_axis) {
    Layout tb = lay("flowchart TB\nA --> B");
    Layout bt = lay("flowchart BT\nA --> B");
    Layout lr = lay("flowchart LR\nA --> B");
    Layout rl = lay("flowchart RL\nA --> B");
    ASSERT_TRUE(tb.find_node("B")->center.y > tb.find_node("A")->center.y);
    ASSERT_TRUE(bt.find_node("B")->center.y < bt.find_node("A")->center.y);
    ASSERT_TRUE(lr.find_node("B")->center.x > lr.find_node("A")->center.x);
    ASSERT_TRUE(rl.find_node("B")->center.x < rl.find_node("A")->center.x);
    ASSERT_NEAR(lr.find_node("A")->center.y, lr.find_node("B")->center.y, 1e-6);
    // LR flows along x: gap between boxes is rank_sep.
    ASSERT_NEAR(40.0, lr.find_node("B")->rect().x - lr.find_node("A")->rect().right(), 1e-6);
}

UTEST(flowchart_layout, single_node_sits_at_the_margin) {
    Layout l = lay("flowchart TD\nA");
    ASSERT_EQ(1u, l.nodes.size());
    ASSERT_NEAR(12.0, l.nodes[0].rect().x, 1e-6);
    ASSERT_NEAR(12.0, l.nodes[0].rect().y, 1e-6);
}

UTEST(flowchart_layout, options_are_respected) {
    LayoutOptions opt;
    opt.rank_sep = 100;
    opt.margin = 0;
    Layout l = lay("flowchart TD\nA --> B", opt);
    ASSERT_NEAR(100.0, l.find_node("B")->rect().y - l.find_node("A")->rect().bottom(), 1e-6);
    ASSERT_NEAR(0.0, l.bounds.x, 1e-6);
}

UTEST(flowchart_layout, long_edges_bend_around_intermediate_ranks) {
    Layout l = lay("flowchart TD\nA --> B\nB --> C\nA --> C");
    const EdgeLayout* ac = nullptr;
    for (const EdgeLayout& e : l.edges)
        if (e.id == "L_A_C_0") ac = &e;
    ASSERT_TRUE(ac != nullptr);
    ASSERT_TRUE(ac->points.size() > 2);
    const Rect b = l.find_node("B")->rect();
    for (const Point& p : ac->points) ASSERT_TRUE(!b.contains(p));
}

UTEST(flowchart_layout, cycles_and_self_loops_terminate) {
    Layout l = lay("flowchart TD\nA --> B\nB --> C\nC --> A\nC --> C");
    ASSERT_EQ(3u, l.nodes.size());
    ASSERT_EQ(4u, l.edges.size());
    ASSERT_TRUE(l.edges[3].points.size() >= 4);  // self loop
    for (const EdgeLayout& e : l.edges) ASSERT_TRUE(e.points.size() >= 2);
}

UTEST(flowchart_layout, edge_endpoints_touch_node_borders) {
    Layout l = lay("flowchart TD\nA[Start] --> B{Choice}\nB --> C((Round))\nA --> C");
    for (const EdgeLayout& e : l.edges) {
        ASSERT_TRUE(e.points.size() >= 2);
        for (const Point& p : e.points) {
            ASSERT_TRUE(std::isfinite(p.x));
            ASSERT_TRUE(std::isfinite(p.y));
        }
    }
    // A --> B ends on B's diamond: |dx|/(w/2) + |dy|/(h/2) == 1
    const NodeLayout* b = l.find_node("B");
    const Point end = l.edges[0].points.back();
    const double k = std::abs(end.x - b->center.x) / (b->size.width / 2) +
                     std::abs(end.y - b->center.y) / (b->size.height / 2);
    ASSERT_NEAR(1.0, k, 1e-6);
}

UTEST(flowchart_layout, subgraph_boxes_enclose_their_members) {
    Layout l = lay(
        "flowchart TD\n"
        "subgraph outer [Outer]\n"
        "  a --> b\n"
        "  subgraph inner [Inner]\n"
        "    c\n"
        "  end\n"
        "end\n"
        "z --> a\n");
    ASSERT_EQ(2u, l.clusters.size());
    const ClusterLayout* outer = nullptr;
    const ClusterLayout* inner = nullptr;
    for (const ClusterLayout& c : l.clusters) (c.id == "outer" ? outer : inner) = &c;
    ASSERT_TRUE(outer && inner);
    ASSERT_STREQ("Outer", outer->title.c_str());
    for (const char* id : {"a", "b", "c"}) {
        const Rect r = l.find_node(id)->rect();
        ASSERT_TRUE(outer->rect.contains({r.x, r.y}));
        ASSERT_TRUE(outer->rect.contains({r.right(), r.bottom()}));
    }
    ASSERT_TRUE(inner->rect.contains({l.find_node("c")->rect().x, l.find_node("c")->rect().y}));
    ASSERT_TRUE(outer->rect.contains({inner->rect.x, inner->rect.y}));
    ASSERT_TRUE(outer->rect.contains({inner->rect.right(), inner->rect.bottom()}));
    ASSERT_TRUE(l.bounds.contains({outer->rect.x, outer->rect.y}));
}

UTEST(flowchart_layout, edges_to_subgraphs_attach_to_a_member) {
    Layout l = lay("flowchart TD\nsubgraph S\n a\nend\nx --> S");
    ASSERT_EQ(2u, l.nodes.size());  // the subgraph id is not a node
    ASSERT_TRUE(l.find_node("S") == nullptr);
    ASSERT_EQ(1u, l.edges.size());
    ASSERT_TRUE(l.edges[0].points.size() >= 2);
}

UTEST(flowchart_layout, fixtures_have_finite_non_overlapping_layouts) {
    const char* names[] = {
        "basic", "class_style", "stress_flowchart_amp_groups_edge_ids_034",
        "stress_flowchart_classdef_and_inline_classes_003",
        "stress_flowchart_comments_and_directives_010",
        "stress_flowchart_deeply_nested_clusters_019",
        "stress_flowchart_dense_parallel_edges_002",
        "stress_flowchart_directives_comments_blank_lines_032",
        "stress_flowchart_edge_label_position_064",
        "stress_flowchart_escape_sequences_and_quotes_012"};
    for (const char* name : names) {
        Layout l = lay(read_fixture(name));
        ASSERT_TRUE(!l.nodes.empty());
        for (size_t i = 0; i < l.nodes.size(); ++i) {
            const Rect ri = l.nodes[i].rect();
            ASSERT_TRUE(std::isfinite(ri.x) && std::isfinite(ri.y));
            ASSERT_TRUE(ri.x >= l.bounds.x - 1e-6 && ri.right() <= l.bounds.right() + 1e-6);
            ASSERT_TRUE(ri.y >= l.bounds.y - 1e-6 && ri.bottom() <= l.bounds.bottom() + 1e-6);
            for (size_t j = i + 1; j < l.nodes.size(); ++j)
                ASSERT_TRUE(!overlaps(ri, l.nodes[j].rect()));
        }
        for (const EdgeLayout& e : l.edges) {
            ASSERT_TRUE(e.points.size() >= 2);
            for (const Point& p : e.points) ASSERT_TRUE(std::isfinite(p.x) && std::isfinite(p.y));
        }
    }
}
