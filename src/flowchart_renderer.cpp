#include <mermaid/render.h>

#include <algorithm>

#include "edge_renderer.h"
#include "flowchart_css.h"
#include "node_renderer.h"
#include "rough_path.h"
#include "style_css.h"
#include "svg_writer.h"
#include "text_block.h"
#include "theme.h"
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
          theme_(detail::Theme::named(options.theme)),
          line_height_(theme_.font_size * 1.5),
          edges_(theme_, options.svg_id, line_height_) {}

    std::string render() {
        open_svg();
        write_style();
        out_.open("g");  // holds the markers and the root group
        edges_.write_markers(out_);
        write_root(layout_, nullptr);
        edges_.write_colored_markers(out_);
        out_.close();  // marker group
        write_filters();
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

    std::string style_sheet() const {
        if (options_.theme == "default") {
            detail::theme_variables vars = detail::theme_variables::redux_color();
            const InitConfig& init = model_.config;  // themeVariables of the `%%{init}%%` directive
            if (init.font_family) vars.font_family = *init.font_family;
            if (init.cluster_bkg) vars.cluster_bkg = *init.cluster_bkg;
            if (init.cluster_border) vars.cluster_border = *init.cluster_border;
            return detail::flowchart_stylesheet(vars, options_.svg_id, model_, options_.css);
        }
        const std::string id = "#" + options_.svg_id;
        std::string css;
        css += id + "{font-family:" + theme_.font_family + ";font-size:" + SvgWriter::num(theme_.font_size) +
               "px;fill:" + theme_.text + ";}";
        css += id + " .node rect," + id + " .node polygon," + id + " .node circle," + id + " .node path{fill:" +
               theme_.node_fill + ";stroke:" + theme_.node_border + ";stroke-width:1px;}";
        css += id + " .cluster rect{fill:" + theme_.cluster_fill + ";stroke:" + theme_.cluster_border +
               ";stroke-width:1px;}";
        css += id + " .flowchart-link{stroke:" + theme_.line + ";stroke-width:2px;fill:none;}";
        css += id + " .edgeLabel .labelBkg{opacity:0.5;}";
        css += options_.css;
        return css;
    }

    void write_style() {
        out_.open("style");
        out_.raw(style_sheet());
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
                                       {"flood-color", "#000000"}});
            out_.close();
            out_.close();
        }
    }

    // rendering-elements/clusters.js `rect` in the neo look.
    void write_clusters(const Layout& layout) {
        const detail::theme_variables vars = detail::theme_variables::redux_color();
        const size_t slots = vars.border_color_array.size();
        out_.open("g", {{"class", "clusters"}});
        for (auto it = layout.clusters.rbegin(); it != layout.clusters.rend(); ++it) {  // outermost first
            const Rect& r = it->rect;
            SvgWriter::Attrs attrs{{"class", "cluster"}, {"id", options_.svg_id + "-" + it->id}, {"data-look", "neo"}};
            if (slots > 0) attrs.emplace_back("data-color-id", "color-" + std::to_string(static_cast<size_t>(it->color_index) % slots));
            out_.open("g", attrs);
            out_.leaf("rect", {{"style", ""},
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
        out_.open("div", {{"xmlns", "http://www.w3.org/1999/xhtml"},
                          {"style", "display: table-cell; white-space: nowrap; line-height: 1.5;"}});
        out_.open("span", {{"class", "nodeLabel"}});
        out_.element("p", {}, c.title);
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
                edges_.write_path(out_, edge, e, find_layout(layout, edge.from), find_layout(layout, edge.to));
            }
        }
        out_.close();
    }

    static const NodeLayout* find_layout(const Layout& layout, const std::string& id) {
        for (const NodeLayout& nl : layout.nodes)
            if (nl.id == id) return &nl;
        return nullptr;  // a subgraph
    }

    void write_edge_labels(const Layout& layout) {
        out_.open("g", {{"class", "edgeLabels"}});
        for (const EdgeLayout& e : layout.edges) edges_.write_label(out_, model_.edges[e.model_index], e);
        out_.close();
    }

    // classDef styles first (default, then the node's classes), then `style` statements.
    Style resolved_style(const Node& node) const {
        Style merged;
        auto add = [&](const std::string& name) {
            const auto it = model_.class_defs.find(name);
            if (it != model_.class_defs.end()) merged.insert(merged.end(), it->second.style.begin(), it->second.style.end());
        };
        add("default");
        for (const std::string& cls : node.classes) add(cls);
        merged.insert(merged.end(), node.style.begin(), node.style.end());

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
        const detail::node_renderer renderer(options_.svg_id, detail::theme_variables::redux_color(), options_.hand_drawn_seed);
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
    detail::Theme theme_;
    double line_height_;
    detail::EdgeRenderer edges_;
    SvgWriter out_;
};

}  // namespace

std::string render_svg(const Model& model, const Layout& layout, const RenderOptions& options) {
    return FlowchartRenderer(model, layout, options).render();
}

}  // namespace mermaid::flowchart
