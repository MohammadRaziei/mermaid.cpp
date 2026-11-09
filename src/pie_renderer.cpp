#include <mermaid/pie_renderer.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <mermaid/css_color.h>
#include <mermaid/error.h>

#include "d3_shape.h"
#include "flowchart_css.h"
#include "rough_path.h"
#include "svg_text_metrics.h"
#include "svg_writer.h"
#include "theme_variables.h"

namespace mermaid::pie {

namespace {

using detail::js_number;
using detail::SvgWriter;

constexpr double kMargin = 40;
constexpr double kLegendRectSize = 18;
constexpr double kLegendSpacing = 4;
constexpr double kHeight = 450;
constexpr double kPieWidth = kHeight;
constexpr size_t kColorCount = 12;

// Number.prototype.toFixed(0): the integer nearest to x, the larger one when two are as near (so 0.5 gives 1, unlike printf)
std::string to_fixed_0(double x) {
    const double whole = std::floor(x);
    return js_number(x - whole >= 0.5 ? whole + 1 : whole);
}

double px(const std::string& css_size) { return std::atof(css_size.c_str()); }

class Renderer {
public:
    Renderer(const Model& model, const RenderOptions& options)
        : model_(model), options_(options), theme_(detail::theme_variables::classic_default()) {}

    std::string run() {
        check_supported();
        const Config& c = model_.config;
        const double text_position = c.text_position.value_or(0.75);
        const double donut_hole = c.donut_hole.value_or(0);
        const std::string legend_position = c.legend_position.value_or("right");
        const double outer_stroke_width = px(theme_.pie.at("pieOuterStrokeWidth"));
        const double inner_hole = donut_hole > 0 && donut_hole <= 0.9 ? donut_hole : 0;
        const double radius = std::min(kPieWidth, kHeight) / 2 - kMargin;
        const detail::d3_arc arc(inner_hole * radius, radius);
        const detail::d3_arc label_arc(radius * text_position, radius * text_position);

        double sum = 0;
        for (const Section& s : model_.sections) sum += s.value;
        // createPieArcs: the sections that are at least 1% of the whole share the circle; the labels of those that round to 0% are dropped
        std::vector<const Section*> shown;
        std::vector<double> values;
        for (const Section& s : model_.sections)
            if (s.value / sum * 100 >= 1) {
                shown.push_back(&s);
                values.push_back(s.value);
            }
        const std::vector<detail::pie_slice> slices = detail::d3_pie(values);
        std::vector<size_t> drawn;
        for (size_t i = 0; i < shown.size(); ++i)
            if (to_fixed_0(shown[i]->value / sum * 100) != "0") drawn.push_back(i);

        const double legend_text_size = px(theme_.pie.at("pieLegendTextSize"));
        double longest = model_.sections.empty() ? 0 : -1;
        for (const Section& s : model_.sections) longest = std::max(longest, detail::svg_text_metrics::sans_bbox_width(legend_label(s), legend_text_size));
        const double title_width = detail::svg_text_metrics::sans_bbox_width(model_.title, px(theme_.pie.at("pieTitleTextSize")));

        const double legend_height = kLegendRectSize + kLegendSpacing;
        const double total_legend_height = static_cast<double>(model_.sections.size()) * legend_height;
        const double count = static_cast<double>(model_.sections.size());
        double height = kHeight, width = kPieWidth + kMargin;
        std::string pie_transform;
        std::vector<std::string> legend_transforms;
        for (size_t i = 0; i < model_.sections.size(); ++i) {
            const double index = static_cast<double>(i);
            if (legend_position == "center") {
                const double offset = legend_height * count / 2;
                legend_transforms.push_back(translate(-longest / 2 - (kLegendRectSize + kLegendSpacing), index * legend_height - offset, false));
            } else if (legend_position == "top") {
                legend_transforms.push_back(translate(-longest / 2 - (kLegendRectSize + kLegendSpacing), index * legend_height - radius, true));
            } else if (legend_position == "bottom") {
                const double offset = -radius - legend_height;
                legend_transforms.push_back(translate(-longest / 2 - (kLegendRectSize + kLegendSpacing), index * legend_height - offset, false));
            } else if (legend_position == "left") {
                const double offset = legend_height * count / 2;
                legend_transforms.push_back(translate(-radius - (kLegendRectSize + kLegendSpacing), index * legend_height - offset, false));
            } else {
                const double offset = legend_height * count / 2;
                legend_transforms.push_back(translate(12 * kLegendRectSize, index * legend_height - offset, false));
            }
        }
        if (legend_position == "top") {
            height += total_legend_height;
            pie_transform = "translate(0, " + js_number(total_legend_height + legend_height) + ")";
        } else if (legend_position == "bottom") {
            height += total_legend_height;
        } else if (legend_position == "left") {
            width += kLegendRectSize + kLegendSpacing + longest;
            pie_transform = "translate(" + js_number(longest + kLegendRectSize + kLegendSpacing) + ", 0)";
        } else if (legend_position != "center") {
            width += kLegendRectSize + kLegendSpacing + longest;
        }
        const double title_left = kPieWidth / 2 - title_width / 2, title_right = kPieWidth / 2 + title_width / 2;
        const double view_box_x = std::min(0.0, title_left);
        const double total_width = std::max(width, title_right) - view_box_x;

        SvgWriter out;
        const std::string& id = options_.svg_id;
        const std::string view_box = js_number(view_box_x) + " 0 " + js_number(total_width) + " " + js_number(height);
        SvgWriter::Attrs frame{{"id", id}, {"width", c.use_max_width.value_or(true) ? "100%" : js_number(total_width)}, {"xmlns", "http://www.w3.org/2000/svg"},
                               {"xmlns:xlink", "http://www.w3.org/1999/xlink"}, {"viewBox", view_box}};
        if (c.use_max_width.value_or(true)) frame.emplace_back("style", "max-width: " + js_number(total_width) + "px; background-color: " + options_.background + ";");
        else frame.emplace_back("height", js_number(height));
        frame.emplace_back("role", "graphics-document document");
        frame.emplace_back("aria-roledescription", "pie");
        if (!model_.acc_descr.empty()) frame.emplace_back("aria-describedby", "chart-desc-" + id);
        if (!model_.acc_title.empty()) frame.emplace_back("aria-labelledby", "chart-title-" + id);
        out.open("svg", frame);
        if (!model_.acc_title.empty()) out.open("title", {{"id", "chart-title-" + id}}).text(model_.acc_title).close();
        if (!model_.acc_descr.empty()) out.open("desc", {{"id", "chart-desc-" + id}}).text(model_.acc_descr).close();
        out.open("style");
        out.raw(detail::assemble_stylesheet(theme_, id, pie_styles(), ":root { --mermaid-font-family: " + theme_.font_family_config + "}\n"));
        out.close();
        out.open("g");
        out.close();
        out.open("g", {{"transform", "translate(" + js_number(kPieWidth / 2) + "," + js_number(kHeight / 2) + ")"}});
        SvgWriter::Attrs pie_attrs;
        if (!pie_transform.empty()) pie_attrs.emplace_back("transform", pie_transform);
        out.open("g", pie_attrs);
        out.leaf("circle", {{"cx", "0"}, {"cy", "0"}, {"r", js_number(radius + outer_stroke_width / 2)}, {"class", "pieOuterCircle"}});
        for (size_t i : drawn) {
            std::string css_class = "pieCircle";
            if (c.highlight_slice && *c.highlight_slice == "hover") css_class += " highlightedOnHover";
            else if (c.highlight_slice && *c.highlight_slice == shown[i]->label) css_class += " highlighted";
            out.leaf("path", {{"d", arc.path(slices[i])}, {"fill", color_of(shown[i]->label)}, {"class", css_class}});
        }
        for (size_t i : drawn) {
            const auto [x, y] = label_arc.centroid(slices[i]);
            out.element("text", {{"transform", "translate(" + js_number(x) + "," + js_number(y) + ")"}, {"class", "slice"}, {"style", "text-anchor: middle;"}},
                        to_fixed_0(shown[i]->value / sum * 100) + "%");
        }
        out.close();
        out.element("text", {{"x", "0"}, {"y", js_number(-(kHeight - 50) / 2)}, {"class", "pieTitleText"}}, model_.title);
        for (size_t i = 0; i < model_.sections.size(); ++i) {
            const Section& s = model_.sections[i];
            out.open("g", {{"class", "legend"}, {"transform", legend_transforms[i]}});
            const std::string color = browser_color(color_of(s.label));
            out.leaf("rect", {{"width", js_number(kLegendRectSize)}, {"height", js_number(kLegendRectSize)}, {"style", "fill: " + color + "; stroke: " + color + ";"}});
            out.element("text", {{"x", js_number(kLegendRectSize + kLegendSpacing)}, {"y", js_number(kLegendRectSize - kLegendSpacing)}}, legend_label(s));
            out.close();
        }
        out.close();
        out.close();
        return out.str();
    }

private:
    void check_supported() const {
        if (options_.theme != "default") throw UnsupportedDiagram("the theme `" + options_.theme + "` of a pie chart is not drawn yet");
        for (const std::string& key : model_.config.unsupported) throw UnsupportedDiagram("the config key `" + key + "` of a pie chart is not drawn yet");
        if (model_.sections.empty()) throw UnsupportedDiagram("a pie chart without sections");
    }

    std::string legend_label(const Section& s) const { return model_.show_data ? s.label + " [" + js_number(s.value) + "]" : s.label; }

    // d3.scaleOrdinal(colors).domain(labels): the colours in turn, in the order of the sections
    std::string color_of(const std::string& label) const {
        size_t index = 0;
        for (size_t i = 0; i < model_.sections.size(); ++i)
            if (model_.sections[i].label == label) index = i;
        return theme_.pie.at("pie" + std::to_string(index % kColorCount + 1));
    }

    // a colour set through an element's style is reported by the browser as rgb(r, g, b)
    static std::string browser_color(const std::string& color) {
        const auto parsed = parse_css_color(color);
        return parsed ? css_rgb(*parsed) : color;
    }

    static std::string translate(double x, double y, bool spaced) { return "translate(" + js_number(x) + (spaced ? ", " : ",") + js_number(y) + ")"; }

    // diagrams/pie/pieStyles.ts
    std::string pie_styles() const {
        const auto& v = theme_.pie;
        const std::string family = theme_.font_family;
        return "\n  .pieCircle{\n    stroke: " + v.at("pieStrokeColor") + ";\n    stroke-width : " + v.at("pieStrokeWidth") + ";\n    opacity : " + v.at("pieOpacity") + ";\n  }\n" +
               "  .pieCircle.highlighted{\n    scale: 1.05;\n    opacity: 1;\n  }\n" +
               "  .pieCircle.highlightedOnHover:hover{\n    transition-duration: 250ms;\n    scale: 1.05;\n    opacity: 1;\n  }\n" +
               "  .pieOuterCircle{\n    stroke: " + v.at("pieOuterStrokeColor") + ";\n    stroke-width: " + v.at("pieOuterStrokeWidth") + ";\n    fill: none;\n  }\n" +
               "  .pieTitleText {\n    text-anchor: middle;\n    font-size: " + v.at("pieTitleTextSize") + ";\n    fill: " + v.at("pieTitleTextColor") + ";\n    font-family: " + family + ";\n  }\n" +
               "  .slice {\n    font-family: " + family + ";\n    fill: " + v.at("pieSectionTextColor") + ";\n    font-size:" + v.at("pieSectionTextSize") + ";\n    // fill: white;\n  }\n" +
               "  .legend text {\n    fill: " + v.at("pieLegendTextColor") + ";\n    font-family: " + family + ";\n    font-size: " + v.at("pieLegendTextSize") + ";\n  }\n";
    }

    const Model& model_;
    const RenderOptions& options_;
    detail::theme_variables theme_;
};

}  // namespace

std::string render_svg(const Model& model, const RenderOptions& options) { return Renderer(model, options).run(); }

}  // namespace mermaid::pie
