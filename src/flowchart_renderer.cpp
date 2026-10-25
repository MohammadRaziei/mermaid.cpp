#include <mermaid/render.h>
#include <memory>

#include <algorithm>

#include "edge_renderer.h"
#include "flowchart_css.h"
#include "markdown_label.h"
#include "node_renderer.h"
#include "rough_path.h"
#include "style_css.h"
#include "svg_writer.h"
#include "text_block.h"
#include "theme_variables.h"

namespace mermaid::flowchart {

namespace {

using detail::SvgWriter;

class FlowchartRenderer {
public:
    FlowchartRenderer(const Model& model, const Layout& layout, const RenderOptions& options)
        : model_(model),
          layout_(layout),
          options_(options),
          vars_(detail::theme_variables::resolved(model.config, options.theme)),
          line_height_(vars_.font_px() * 1.5),
          edges_(options.svg_id, line_height_) {}

    std::string render() {
        open_svg();
        write_style();
        out_.open("g");  // holds the markers and the root group
        edges_.write_markers(out_);
        write_root(layout_, nullptr);
        edges_.write_colored_markers(out_);
        out_.close();  // marker group
        write_filters();
        write_gradient();
        out_.close();  // svg
        return out_.str();
    }

private:
    // One dagre graph. A closed subgraph is a nested root, moved onto its box in the parent.
    void write_root(const Layout& layout, const Point* origin) {
        SvgWriter::Attrs attrs{{"class", "root"}};
        if (origin) attrs.emplace_back("transform", "translate(" + SvgWriter::num(origin->x) + ", " + SvgWriter::num(origin->y) + ")");
        out_.open("g", attrs);
        write_clusters(layout);
        write_edge_paths(layout);
        write_edge_labels(layout);
        write_nodes(layout);
        out_.close();
    }

    void open_svg() {
        // Mermaid: viewBox = content bounds + `diagramPadding` (8) on every side.
        constexpr double kDiagramPadding = 8;
        const Rect& b = layout_.bounds;
        const std::string w = SvgWriter::num(b.width + 2 * kDiagramPadding);
        const std::string id = options_.svg_id;
        SvgWriter::Attrs attrs{{"id", id},
                               {"width", "100%"},
                               {"xmlns", "http://www.w3.org/2000/svg"},
                               {"xmlns:xlink", "http://www.w3.org/1999/xlink"},
                               {"class", "flowchart"},
                               {"style", "max-width: " + w + "px; background-color: " + options_.background + ";"},
                               {"viewBox", SvgWriter::num(b.x - kDiagramPadding) + " " + SvgWriter::num(b.y - kDiagramPadding) + " " +
                                               w + " " + SvgWriter::num(b.height + 2 * kDiagramPadding)},
                               {"role", "graphics-document document"},
                               {"aria-roledescription", "flowchart-v2"}};
        if (!model_.acc_title.empty()) attrs.emplace_back("aria-labelledby", "chart-title-" + id);
        if (!model_.acc_descr.empty()) attrs.emplace_back("aria-describedby", "chart-desc-" + id);
        out_.open("svg", attrs);
        if (!model_.acc_title.empty()) out_.element("title", {{"id", "chart-title-" + id}}, model_.acc_title);
        if (!model_.acc_descr.empty()) out_.element("desc", {{"id", "chart-desc-" + id}}, model_.acc_descr);
    }

    std::string style_sheet() const { return detail::flowchart_stylesheet(vars_, options_.svg_id, model_, options_.css); }

    void write_style() {
        out_.open("style");
        out_.raw(style_sheet());
        out_.close();
    }

    std::string options_theme_name() const { return model_.config.theme ? *model_.config.theme : options_.theme; }

    // insertLookDefs: the gradient stops of a theme with `useGradient`, after the drop shadows.
    void write_gradient() {
        if (!vars_.use_gradient) return;
        out_.open("linearGradient", {{"id", options_.svg_id + "-gradient"},
                                     {"gradientUnits", "objectBoundingBox"},
                                     {"x1", "0%"},
                                     {"y1", "0%"},
                                     {"x2", "100%"},
                                     {"y2", "0%"}});
        out_.leaf("stop", {{"offset", "0%"}, {"stop-color", vars_.gradient_start}, {"stop-opacity", "1"}});
        out_.leaf("stop", {{"offset", "100%"}, {"stop-color", vars_.gradient_stop}, {"stop-opacity", "1"}});
        out_.close();
    }

    // Drop shadows of the neo look (one <defs> per filter, as Mermaid writes them).
    void write_filters() {
        struct filter_spec {
            const char* suffix;
            const char* size;
            const char* offset;
        };
        for (const filter_spec& f : {filter_spec{"drop-shadow", "130%", "4"}, filter_spec{"drop-shadow-small", "150%", "2"}}) {
            out_.open("defs");
            out_.open("filter", {{"id", options_.svg_id + "-" + f.suffix}, {"height", f.size}, {"width", f.size}});
            out_.leaf("feDropShadow", {{"dx", f.offset},
                                       {"dy", f.offset},
                                       {"stdDeviation", "0"},
                                       {"flood-opacity", "0.06"},
                                       {"flood-color", options_theme_name().find("dark") != std::string::npos ? "#FFFFFF" : "#000000"}});
            out_.close();
            out_.close();
        }
    }

    // rendering-elements/clusters.js `rect` in the neo look.
    void write_clusters(const Layout& layout) {
        const size_t slots = vars_.border_color_array.size();
        out_.open("g", {{"class", "clusters"}});
        for (auto it = layout.clusters.rbegin(); it != layout.clusters.rend(); ++it) {  // outermost first
            const Rect& r = it->rect;
            const Subgraph* subgraph = model_.find_subgraph(it->id);
            std::string classes = "cluster";
            if (subgraph)
                for (const std::string& cls : subgraph->classes) classes += " " + cls;
            SvgWriter::Attrs attrs{{"class", classes}, {"id", options_.svg_id + "-" + it->id}, {"data-look", "neo"}};
            if (slots > 0) attrs.emplace_back("data-color-id", "color-" + std::to_string(static_cast<size_t>(it->color_index) % slots));
            out_.open("g", attrs);
            out_.leaf("rect", {{"style", subgraph ? detail::important_shape_style(resolved_style(*subgraph)) : std::string()},
                               {"x", SvgWriter::num(r.x)},
                               {"y", SvgWriter::num(r.y)},
                               {"width", SvgWriter::num(r.width)},
                               {"height", SvgWriter::num(r.height)}});
            write_cluster_label(*it);
            out_.close();
        }
        out_.close();
    }

    void write_cluster_label(const ClusterLayout& c) {
        out_.open("g", {{"class", "cluster-label"},
                        {"transform", "translate(" + detail::js_number(c.rect.center().x - c.label.width / 2) + ", " +
                                          detail::js_number(c.rect.y) + ")"}});
        out_.open("foreignObject", {{"width", detail::js_number(c.label.width)}, {"height", detail::js_number(c.label.height)}});
        // a Markdown title goes through createText with the width of the cluster: that adds max-width and centring
        out_.open("div", {{"xmlns", "http://www.w3.org/1999/xhtml"},
                          {"style", c.markdown_title ? "display: table-cell; white-space: nowrap; line-height: 1.5; max-width: " +
                                                           detail::js_number(c.rect.width) + "px; text-align: center;"
                                                     : "display: table-cell; white-space: nowrap; line-height: 1.5;"}});
        out_.open("span", {{"class", "nodeLabel"}});
        if (c.markdown_title) out_.raw(detail::markdown_label(c.title).html());
        else out_.element("p", {}, c.title);
        out_.close();  // span
        out_.close();  // div
        out_.close();  // foreignObject
        out_.close();  // cluster-label
    }

    void write_edge_paths(const Layout& layout) {
        out_.open("g", {{"class", "edgePaths"}});
        // getEdgesToRender: the ordinary edges first, the merged self-loops after them
        for (const bool loops : {false, true}) {
            for (const EdgeLayout& e : layout.edges) {
                if (e.self_loop != loops) continue;
                const Edge& edge = model_.edges[e.model_index];
                detail::EdgeRenderer::Ends ends;
                ends.tail = end_layout(layout, edge.from, ends.tail_is_cluster);
                ends.head = end_layout(layout, edge.to, ends.head_is_cluster);
                edges_.write_path(out_, edge, e, ends);
            }
        }
        out_.close();
    }

    static const NodeLayout* find_layout(const Layout& layout, const std::string& id) {
        for (const NodeLayout& nl : layout.nodes)
            if (nl.id == id) return &nl;
        return nullptr;  // a subgraph
    }

    // The node an edge end is drawn against: the node itself, or for a subgraph id the box of its cluster as a
    // rectangle (kept in `cluster_ends_`, which outlives the call).
    const NodeLayout* end_layout(const Layout& layout, const std::string& id, bool& is_cluster) {
        if (const NodeLayout* node = find_layout(layout, id)) return node;
        Rect box;
        bool found = false;
        for (const ClusterLayout& c : layout.clusters)
            if (c.id == id) box = c.rect, found = true;
        for (const RootLayout& r : layout.roots)
            if (r.id == id) box = r.box, found = true;
        if (!found) return nullptr;
        is_cluster = true;
        cluster_ends_.push_back(std::make_unique<NodeLayout>());
        NodeLayout& n = *cluster_ends_.back();
        n.id = id;
        n.center = box.center();
        n.size = {box.width, box.height};
        return &n;
    }

    void write_edge_labels(const Layout& layout) {
        out_.open("g", {{"class", "edgeLabels"}});
        for (const EdgeLayout& e : layout.edges) edges_.write_label(out_, model_.edges[e.model_index], e);
        out_.close();
    }

    // classDef styles first (default, then the node's classes), then `style` statements.
    Style resolved_style(const Node& node) const {
        std::vector<std::string> names{"default"};
        names.insert(names.end(), node.classes.begin(), node.classes.end());
        return collapse(class_styles(names), node.style);
    }

    // A subgraph takes only the styles of its own classes (getCompiledStyles(subGraph.classes)).
    Style resolved_style(const Subgraph& subgraph) const { return collapse(class_styles(subgraph.classes), {}); }

    Style class_styles(const std::vector<std::string>& names) const {
        Style merged;
        for (const std::string& name : names) {
            const auto it = model_.class_defs.find(name);
            if (it != model_.class_defs.end()) merged.insert(merged.end(), it->second.style.begin(), it->second.style.end());
        }
        return merged;
    }

    static Style collapse(Style merged, const Style& own) {
        merged.insert(merged.end(), own.begin(), own.end());

        // Mermaid keeps these in an object: a repeated property keeps its first place and takes the last value.
        Style unique;
        for (const auto& declaration : merged) {
            const auto same = std::find_if(unique.begin(), unique.end(), [&](const auto& d) { return d.first == declaration.first; });
            if (same == unique.end()) unique.push_back(declaration);
            else same->second = declaration.second;
        }
        return unique;
    }

    void write_nodes(const Layout& layout) {
        out_.open("g", {{"class", "nodes"}});
        for (const RootLayout& root : layout.roots) write_root(*root.layout, &root.origin);
        const auto write_loop_helpers = [&] {
            // the helpers of self-loops: empty `labelRect` nodes, drawn after the real nodes
            for (const LoopHelper& h : layout.loop_helpers) {
                out_.open("g", {{"class", "label edgeLabel"},
                                {"id", h.id},
                                {"transform", "translate(" + SvgWriter::num(h.center.x) + ", " + SvgWriter::num(h.center.y) + ")"}});
                out_.leaf("rect", {{"width", "0.1"}, {"height", "0.1"}});
                out_.open("g", {{"class", "label"}, {"style", ""}, {"transform", "translate(0, 0)"}});
                out_.leaf("rect", {});
                out_.open("foreignObject", {{"width", "0"}, {"height", "0"}});
                out_.open("div", {{"xmlns", "http://www.w3.org/1999/xhtml"},
                                  {"style", "display: table-cell; white-space: nowrap; line-height: 1.5; max-width: 10px; text-align: center;"}});
                out_.leaf("span", {{"class", "nodeLabel"}});
                out_.close();  // div
                out_.close();  // foreignObject
                out_.close();  // label
                out_.close();  // helper
            }
        };
        const detail::node_renderer renderer(options_.svg_id, vars_, options_.hand_drawn_seed);
        for (const NodeLayout& nl : layout.nodes) {
            const Node* node = model_.find_node(nl.id);
            if (!node) continue;
            renderer.write(out_, *node, nl, resolved_style(*node));
        }
        write_loop_helpers();
        out_.close();
    }

    const Model& model_;
    const Layout& layout_;
    const RenderOptions& options_;
    detail::theme_variables vars_;
    std::vector<std::unique_ptr<NodeLayout>> cluster_ends_;
    double line_height_;
    detail::EdgeRenderer edges_;
    SvgWriter out_;
};

}  // namespace

std::string render_svg(const Model& model, const Layout& layout, const RenderOptions& options) {
    return FlowchartRenderer(model, layout, options).render();
}

}  // namespace mermaid::flowchart
