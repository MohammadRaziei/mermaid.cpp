#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

#include <mermaid/browser_text_measurer.h>
#include <mermaid/flowchart_layout.h>
#include <mermaid/flowchart_parser.h>
#include "utest.h"

using namespace mermaid;
using namespace mermaid::flowchart;

namespace {

Layout lay(const std::string& src, LayoutOptions opt = {}) {
    static browser_text_measurer measurer;
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

// Numbers below come from the real Mermaid 12.2.0 with `layout: dagre` (tests/data/mermaid_js_dagre).
UTEST(flowchart_layout, basic_matches_mermaid_dagre_nodes) {
    Layout l = lay(read_fixture("basic"));
    ASSERT_EQ(4u, l.nodes.size());
    const NodeLayout* a = l.find_node("A");
    const NodeLayout* b = l.find_node("B");
    const NodeLayout* c = l.find_node("C");
    const NodeLayout* d = l.find_node("D");
    ASSERT_TRUE(a && b && c && d);
    ASSERT_NEAR(185.0, a->center.x, 1e-6);
    ASSERT_NEAR(30.5, a->center.y, 1e-6);
    ASSERT_NEAR(152.0, a->size.width, 1e-6);
    ASSERT_NEAR(45.0, a->size.height, 1e-6);
    ASSERT_NEAR(185.0, b->center.x, 1e-6);
    ASSERT_NEAR(188.5, b->center.y, 1e-6);
    ASSERT_NEAR(171.0, b->size.width, 1e-6);
    ASSERT_NEAR(171.0, b->size.height, 1e-6);
    ASSERT_NEAR(84.0, c->center.x, 1e-6);
    ASSERT_NEAR(367.5, c->center.y, 1e-6);
    ASSERT_NEAR(286.0, d->center.x, 1e-6);
    ASSERT_NEAR(367.5, d->center.y, 1e-6);
}

UTEST(flowchart_layout, basic_matches_mermaid_dagre_bounds_and_edge) {
    Layout l = lay(read_fixture("basic"));
    ASSERT_NEAR(8.0, l.bounds.x, 1e-6);
    ASSERT_NEAR(8.0, l.bounds.y, 1e-6);
    ASSERT_NEAR(362.0, l.bounds.right(), 1e-6);
    ASSERT_NEAR(390.0, l.bounds.bottom(), 1e-6);
    ASSERT_NEAR(370.0, l.size.width, 1e-6);
    ASSERT_NEAR(398.0, l.size.height, 1e-6);
    const EdgeLayout& e = l.edges[0];
    ASSERT_STREQ("L_A_B_0", e.id.c_str());
    ASSERT_EQ(3u, e.points.size());  // Mermaid keeps the point on the empty label rank
    ASSERT_NEAR(185.0, e.points[0].x, 1e-6);
    ASSERT_NEAR(53.0, e.points[0].y, 1e-6);
    ASSERT_NEAR(78.0, e.points[1].y, 1e-6);
    ASSERT_NEAR(185.0, e.points[2].x, 1e-6);
    ASSERT_NEAR(103.0, e.points[2].y, 1e-6);
}

UTEST(flowchart_layout, class_style_matches_mermaid_dagre) {
    Layout l = lay(read_fixture("class_style"));
    const NodeLayout* a = l.find_node("A");
    const NodeLayout* b = l.find_node("B");
    ASSERT_NEAR(84.0, a->center.x, 1e-6);
    ASSERT_NEAR(30.5, a->center.y, 1e-6);
    ASSERT_NEAR(84.0, b->center.x, 1e-6);
    ASSERT_NEAR(125.5, b->center.y, 1e-6);
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
    ASSERT_NEAR(50.0, lr.find_node("B")->rect().x - lr.find_node("A")->rect().right(), 1e-6);
}

UTEST(flowchart_layout, single_node_sits_at_the_margin) {
    Layout l = lay("flowchart TD\nA");
    ASSERT_EQ(1u, l.nodes.size());
    ASSERT_NEAR(8.0, l.nodes[0].rect().x, 1e-6);
    ASSERT_NEAR(8.0, l.nodes[0].rect().y, 1e-6);
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
        "z --> a\n"
        "z --> c\n");  // an edge into each box keeps both of them in the root graph (see closed_clusters_...)
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

UTEST(flowchart_layout, edge_to_a_closed_subgraph_ends_on_its_box) {
    Layout l = lay("flowchart TD\nsubgraph S\n a\nend\nx --> S");
    ASSERT_EQ(1u, l.nodes.size());  // the subgraph id is not a node; `a` lives in the nested root
    ASSERT_TRUE(l.find_node("S") == nullptr);
    ASSERT_EQ(1u, l.roots.size());
    ASSERT_EQ(1u, l.edges.size());
    ASSERT_TRUE(l.edges[0].points.size() >= 2);
    ASSERT_NEAR(l.roots[0].box.y, l.edges[0].points.back().y, 1e-6);  // the arrow stops at the top of the box
}

UTEST(flowchart_layout, edge_into_a_member_keeps_the_subgraph_in_the_parent_graph) {
    Layout l = lay("flowchart TD\nsubgraph S\n a\nend\nx --> S\nS --> a2\nz --> a\n");
    ASSERT_EQ(0u, l.roots.size());
    ASSERT_TRUE(l.find_node("a") != nullptr);
    ASSERT_EQ(1u, l.clusters.size());
}

// Every graph (the root and each nested root): finite, inside its bounds, nothing overlapping.
static bool graph_is_sound(const Layout& l, size_t& nodes) {
    nodes += l.nodes.size();
    for (size_t i = 0; i < l.nodes.size(); ++i) {
        const Rect ri = l.nodes[i].rect();
        if (!std::isfinite(ri.x) || !std::isfinite(ri.y)) return false;
        if (ri.x < l.bounds.x - 1e-6 || ri.right() > l.bounds.right() + 1e-6) return false;
        if (ri.y < l.bounds.y - 1e-6 || ri.bottom() > l.bounds.bottom() + 1e-6) return false;
        for (size_t j = i + 1; j < l.nodes.size(); ++j)
            if (overlaps(ri, l.nodes[j].rect())) return false;
    }
    for (const EdgeLayout& e : l.edges) {
        if (e.points.size() < 2) return false;
        for (const Point& p : e.points)
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
    }
    for (const RootLayout& r : l.roots)
        if (!graph_is_sound(*r.layout, nodes)) return false;
    return true;
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
        const Layout l = lay(read_fixture(name));
        size_t nodes = 0;
        ASSERT_TRUE(graph_is_sound(l, nodes));
        ASSERT_TRUE(nodes > 0);
    }
}

// Golden: stress_flowchart_classdef_and_inline_classes_003 (LR). Positioning comes from the
// dagre pipeline (Brandes-Koepf); the first three nodes share one y and fixed x.
UTEST(flowchart_layout, dagre_positions_match_mermaid_dagre_for_the_main_chain) {
    Layout l = lay(read_fixture("stress_flowchart_classdef_and_inline_classes_003"));
    const NodeLayout* in = l.find_node("in");
    const NodeLayout* parse = l.find_node("parse");
    const NodeLayout* validate = l.find_node("validate");
    ASSERT_TRUE(in && parse && validate);
    ASSERT_NEAR(84.0, in->center.x, 1e-3);
    ASSERT_NEAR(286.0, parse->center.x, 1e-3);
    ASSERT_NEAR(497.5, validate->center.x, 1e-3);
    ASSERT_NEAR(108.1115, in->center.y, 1e-3);
    ASSERT_NEAR(108.1115, parse->center.y, 1e-3);
    ASSERT_NEAR(108.1115, validate->center.y, 1e-3);
}

// Golden: the `store[(Store)]` cylinder of fixture 003 is 144 x 85.149
// (w = 120 + 24, ry = w/2 / (2.5 + w/50), h = 21 + 24 + 3*ry).
UTEST(flowchart_layout, cylinder_size_matches_merman_golden) {
    Layout l = lay("flowchart LR\nstore[(Store)]");
    const NodeLayout* n = l.find_node("store");
    ASSERT_TRUE(n != nullptr);
    ASSERT_NEAR(144.0, n->size.width, 1e-3);
    ASSERT_NEAR(85.149, n->size.height, 1e-3);
}

// shapes/stadium.ts: h = text + 24; w = max(label + h / 4 + 40, ...). A short label is widened to 120 first,
// so the node is 120 + 11.25 + 40 wide (171.25; Mermaid's drawn bbox is 0.02 narrower).
UTEST(flowchart_layout, stadium_is_wider_than_a_box_by_the_cap_allowance) {
    const Layout l = lay("flowchart LR\nA([end])");
    const NodeLayout* a = l.find_node("A");
    ASSERT_TRUE(a != nullptr);
    ASSERT_NEAR(171.25, a->size.width, 0.05);
    ASSERT_NEAR(45.0, a->size.height, 1e-9);
}

UTEST(flowchart_layout, init_directive_sets_node_and_rank_spacing) {
    const Layout base = lay("flowchart TD\nA-->B\nA-->C\n");
    const Layout wide = lay("%%{init: {\"flowchart\": {\"nodeSpacing\": 70, \"rankSpacing\": 90}}}%%\nflowchart TD\nA-->B\nA-->C\n");
    const double base_gap = base.find_node("C")->center.x - base.find_node("B")->center.x;
    const double wide_gap = wide.find_node("C")->center.x - wide.find_node("B")->center.x;
    ASSERT_EQ(20.0, wide_gap - base_gap);
    const double base_rank = base.find_node("B")->center.y - base.find_node("A")->center.y;
    const double wide_rank = wide.find_node("B")->center.y - wide.find_node("A")->center.y;
    ASSERT_EQ(40.0, wide_rank - base_rank);
}

// ---- extracted subgraphs (Mermaid's recursive render): a cluster with no edge leaving it is its own dagre graph

static const char* kTwoClusters =
    "%%{init: {\"flowchart\": {\"nodeSpacing\": 70, \"rankSpacing\": 60}} }%%\nflowchart TB\n"
    "subgraph ClusterA\n  direction LR\n  a1[One] --> a2[Two]\nend\n"
    "subgraph ClusterB\n  direction LR\n  b1[Three] --> b2[Four]\nend\n"
    "ClusterA --> ClusterB\n";

UTEST(flowchart_layout, closed_clusters_are_nested_roots_in_reverse_declaration_order) {
    const Layout l = lay(kTwoClusters);
    ASSERT_EQ(2u, l.roots.size());
    ASSERT_STREQ("ClusterB", l.roots[0].id.c_str());
    ASSERT_STREQ("ClusterA", l.roots[1].id.c_str());
    ASSERT_EQ(0u, l.nodes.size());  // the nodes live in the roots
    ASSERT_EQ(1u, l.edges.size());
}

UTEST(flowchart_layout, nested_root_is_laid_out_in_its_own_direction_and_coordinates) {
    const Layout l = lay(kTwoClusters);
    const RootLayout& a = l.roots[1];
    // LR inside: the nodes sit side by side, 152 wide with 85 (rankSpacing + 25) between them.
    const NodeLayout* a1 = a.layout->find_node("a1");
    const NodeLayout* a2 = a.layout->find_node("a2");
    ASSERT_TRUE(a1 && a2);
    ASSERT_EQ(a1->center.y, a2->center.y);
    ASSERT_NEAR(237.0, a2->center.x - a1->center.x, 1e-6);
    // The box of the cluster is the node the parent graph sees; the root group is moved onto it.
    const Rect inner = a.layout->clusters.back().rect;
    ASSERT_NEAR(474.0, inner.width, 1e-6);
    ASSERT_NEAR(135.0, inner.height, 1e-6);
    ASSERT_NEAR(a.box.x, a.origin.x + inner.x, 1e-6);
    ASSERT_NEAR(a.box.y, a.origin.y + inner.y, 1e-6);
}

UTEST(flowchart_layout, edge_between_closed_clusters_runs_box_to_box) {
    const Layout l = lay(kTwoClusters);
    const EdgeLayout& e = l.edges[0];
    ASSERT_NEAR(245.0, e.points.front().x, 1e-6);
    ASSERT_NEAR(143.0, e.points.front().y, 1e-6);  // bottom of ClusterA
    ASSERT_NEAR(203.0, e.points.back().y, 1e-6);   // top of ClusterB
    ASSERT_NEAR(490.0, l.size.width, 1e-6);
    ASSERT_NEAR(346.0, l.size.height, 1e-6);
}

UTEST(flowchart_layout, cluster_with_an_edge_leaving_it_stays_in_the_parent_graph) {
    const Layout l = lay("flowchart TB\nsubgraph S\n a --> b\nend\nb --> c\n");
    ASSERT_EQ(0u, l.roots.size());
    ASSERT_TRUE(l.find_node("a") != nullptr);
    ASSERT_EQ(1u, l.clusters.size());
}

UTEST(flowchart_layout, closed_cluster_without_direction_flips_the_parent_direction) {
    const Layout l = lay("flowchart TB\nsubgraph S\n a --> b\nend\nx --> y\n");
    ASSERT_EQ(1u, l.roots.size());
    const NodeLayout* a = l.roots[0].layout->find_node("a");
    const NodeLayout* b = l.roots[0].layout->find_node("b");
    ASSERT_EQ(a->center.y, b->center.y);  // LR inside a TB diagram
}

// ---- a cluster box holds exactly its members (S6b)

static std::set<std::string> members_below(const Model& m, const std::string& sub) {
    std::set<std::string> out;
    for (const Subgraph& s : m.subgraphs) {
        if (s.id != sub) continue;
        for (const std::string& id : s.members) {
            out.insert(id);
            const std::set<std::string> deeper = members_below(m, id);
            out.insert(deeper.begin(), deeper.end());
        }
    }
    return out;
}

// In every graph: members lie inside the box of their cluster, other nodes do not touch it.
static bool clusters_hold_exactly_their_members(const Model& m, const Layout& l) {
    for (const ClusterLayout& c : l.clusters) {
        const std::set<std::string> members = members_below(m, c.id);
        for (const NodeLayout& n : l.nodes) {
            const Rect r = n.rect();
            const bool inside = r.x >= c.rect.x - 0.01 && r.right() <= c.rect.right() + 0.01 && r.y >= c.rect.y - 0.01 &&
                                r.bottom() <= c.rect.bottom() + 0.01;
            const bool touches = r.x < c.rect.right() && r.right() > c.rect.x && r.y < c.rect.bottom() && r.bottom() > c.rect.y;
            if (members.count(n.id) ? !inside : touches) return false;
        }
    }
    for (const RootLayout& r : l.roots)
        if (!clusters_hold_exactly_their_members(m, *r.layout)) return false;
    return true;
}

static bool cluster_boxes_are_clean(const std::string& source) {
    static browser_text_measurer measurer;
    const Model m = Parser().parse(source);
    return clusters_hold_exactly_their_members(m, LayoutEngine(measurer, LayoutOptions{}).layout(m));
}

UTEST(flowchart_layout, cluster_box_holds_exactly_its_members_when_outside_nodes_share_its_ranks) {
    ASSERT_TRUE(cluster_boxes_are_clean(
        "flowchart TD\n top --> a\n top --> out1\n subgraph S\n  a --> b --> c\n end\n out1 --> out2 --> out3\n c --> end1\n out3 --> end1\n"));
}

UTEST(flowchart_layout, nested_cluster_boxes_hold_exactly_their_members_left_to_right) {
    ASSERT_TRUE(cluster_boxes_are_clean(
        "flowchart LR\n x --> p\n subgraph A\n  p --> q\n  subgraph B\n   q --> r\n  end\n end\n x --> y --> z --> r\n r --> w\n y --> q\n"));
}

UTEST(flowchart_layout, cluster_boxes_of_the_fixtures_hold_exactly_their_members) {
    ASSERT_TRUE(cluster_boxes_are_clean(read_fixture("stress_flowchart_deeply_nested_clusters_019")));
    ASSERT_TRUE(cluster_boxes_are_clean(read_fixture("stress_flowchart_comments_and_directives_010")));
    ASSERT_TRUE(cluster_boxes_are_clean(read_fixture("stress_flowchart_directives_comments_blank_lines_032")));
}
