#include <mermaid/render.h>

#include <algorithm>

#include "edge_renderer.h"
#include "node_renderer.h"
#include "shape_renderer.h"
#include "style_css.h"
#include "svg_writer.h"
#include "text_block.h"
#include "theme.h"

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
        out_.open("g", {{"class", "root"}});
        write_clusters();
        write_edge_paths();
        write_edge_labels();
        write_nodes();
        out_.close();  // root
        out_.close();  // marker group
        write_filters();
        out_.close();  // svg
        return out_.str();
    }

private:
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
        struct Filter {
            const char* suffix;
            const char* size;
            const char* offset;
        };
        for (const Filter& f : {Filter{"drop-shadow", "130%", "4"}, Filter{"drop-shadow-small", "150%", "2"}}) {
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

    void write_clusters() {
        out_.open("g", {{"class", "clusters"}});
        for (auto it = layout_.clusters.rbegin(); it != layout_.clusters.rend(); ++it) {  // outermost first
            const Rect& r = it->rect;
            out_.open("g", {{"class", "cluster"}, {"id", it->id}});
            out_.leaf("rect", {{"x", SvgWriter::num(r.x)},
                               {"y", SvgWriter::num(r.y)},
                               {"width", SvgWriter::num(r.width)},
                               {"height", SvgWriter::num(r.height)}});
            if (!it->title.empty())
                detail::write_text_block(out_, {r.center().x, r.y + line_height_ / 2 + 4}, it->title, line_height_);
            out_.close();
        }
        out_.close();
    }

    void write_edge_paths() {
        out_.open("g", {{"class", "edgePaths"}});
        for (size_t i = 0; i < model_.edges.size() && i < layout_.edges.size(); ++i)
            edges_.write_path(out_, model_.edges[i], layout_.edges[i]);
        out_.close();
    }

    void write_edge_labels() {
        out_.open("g", {{"class", "edgeLabels"}});
        for (size_t i = 0; i < model_.edges.size() && i < layout_.edges.size(); ++i)
            edges_.write_label(out_, model_.edges[i], layout_.edges[i]);
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
        return merged;
    }

    void write_nodes() {
        out_.open("g", {{"class", "nodes"}});
        const detail::NodeRenderer renderer(options_.svg_id);
        for (const NodeLayout& nl : layout_.nodes) {
            const Node* node = model_.find_node(nl.id);
            if (!node) continue;
            renderer.write(out_, *node, nl, resolved_style(*node));
        }
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
