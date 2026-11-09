#include <cctype>
#include <mermaid/sequence_renderer.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>

#include <mermaid/css_color.h>
#include <mermaid/error.h>
#include <mermaid/macros.h>

#include "rough_path.h"  // js_number
#include "sequence_css.h"
#include "sequence_defs.h"
#include "sequence_icons.h"
#include "svg_label.h"
#include "svg_text_metrics.h"
#include "svg_writer.h"
#include "entity_codes.h"
#include "theme_variables.h"

namespace mermaid::sequence {

namespace {

using detail::SvgWriter;

// The `sequence` config of Mermaid 12.2 (defaultConfig.ts) that the drawing reads: the defaults, then the init directives.
struct conf {
    double width = 150, height = 65, actor_margin = 50, box_margin = 10, box_text_margin = 5, note_margin = 10, wrap_padding = 10;
    double activation_width = 10, label_box_width = 50, label_box_height = 20;
    double font_size = 16;  // Mermaid's top-level `fontSize`
    double diagram_margin_x = 50, diagram_margin_y = 10, bottom_margin_adj = 1;
    bool mirror_actors = true, show_sequence_numbers = false, use_max_width = true;
    bool neo = true;  // look: neo (the default) or classic
    bool right_angles = false, hide_unused_participants = false;
    std::string message_align = "center", note_align = "center";

    explicit conf(const Config& c) {
        const auto set = [](double& target, const std::optional<double>& v) { if (v) target = *v; };
        set(width, c.width); set(height, c.height); set(actor_margin, c.actor_margin); set(box_margin, c.box_margin);
        set(box_text_margin, c.box_text_margin); set(note_margin, c.note_margin); set(wrap_padding, c.wrap_padding);
        set(activation_width, c.activation_width); set(label_box_width, c.label_box_width); set(label_box_height, c.label_box_height);
        set(diagram_margin_x, c.diagram_margin_x); set(diagram_margin_y, c.diagram_margin_y); set(bottom_margin_adj, c.bottom_margin_adj);
        set(font_size, c.font_size);
        mirror_actors = c.mirror_actors.value_or(true);
        show_sequence_numbers = c.show_sequence_numbers.value_or(false);
        use_max_width = c.use_max_width.value_or(true);
        neo = c.look.value_or("neo") != "classic";
        right_angles = c.right_angles.value_or(false);
        hide_unused_participants = c.hide_unused_participants.value_or(false);
        message_align = c.message_align.value_or("center");
        note_align = c.note_align.value_or("center");
    }
};
constexpr double kGlyphBand = 44, kGlyphGap = 6, kLifelineGap = 6;  // actorBands.ts
constexpr double kNeoOffset = 3;                    // arrows stop 3px before the lifeline in the neo look
constexpr double kExtraHeightForNeoActors = 30;

std::string num(double v) { return detail::js_number(v); }

// JavaScript's Math.round: halves go up, also for negative numbers (std::round sends -108.5 to -109)
double js_round(double v) { return std::floor(v + 0.5); }

// sanitizeText runs the title through DOMPurify, whose HTML serialisation writes a bare `&` as `&amp;`
// (tags and finished entities stay as they are; the SVG writer escapes the result once more when it is set as text).
std::string purify_ampersands(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&') {
            size_t j = i + 1;
            if (j < text.size() && text[j] == '#') ++j;
            const size_t start = j;
            while (j < text.size() && std::isalnum(static_cast<unsigned char>(text[j]))) ++j;
            if (j > start && j < text.size() && text[j] == ';') {
                out += text[i];
                continue;
            }
            out += "&amp;";
        } else {
            out += text[i];
        }
    }
    return out;
}

// the half arrows (41..48) and their dotted forms (51..58)
bool is_half_arrow(LineType t) {
    const int v = static_cast<int>(t);
    return (v >= 41 && v <= 48) || (v >= 51 && v <= 58);
}

// a half arrow whose head sits at the start of the line
bool is_reverse_arrow(LineType t) {
    const int v = static_cast<int>(t) % 10;
    return is_half_arrow(t) && v >= 5;
}

// the half arrows with a solid head: SolidTop/Bottom and the solid reverse ones (the stick heads do not shorten the line)
bool is_solid_reverse_arrow(LineType t) {
    const int v = static_cast<int>(t) % 10;
    return is_half_arrow(t) && (v == 5 || v == 6);
}

bool is_stick_arrow(LineType t) {
    const int v = static_cast<int>(t) % 10;
    return is_half_arrow(t) && (v == 3 || v == 4 || v == 7 || v == 8);
}

// `activate X` and the activations a central connection `()` starts
bool is_activation_start(LineType t) { return t == LineType::ActiveStart || t == LineType::CentralConnection || t == LineType::CentralConnectionReverse; }

bool is_arrow(LineType t) {
    if (is_half_arrow(t)) return true;
    switch (t) {
        case LineType::Solid: case LineType::Dotted: case LineType::SolidCross: case LineType::DottedCross:
        case LineType::SolidOpen: case LineType::DottedOpen: case LineType::SolidPoint: case LineType::DottedPoint:
        case LineType::BidirectionalSolid: case LineType::BidirectionalDotted:
            return true;
        default:
            return false;
    }
}

bool is_dotted(LineType t) {
    return t == LineType::Dotted || t == LineType::DottedCross || t == LineType::DottedPoint || t == LineType::DottedOpen ||
           t == LineType::BidirectionalDotted || (is_half_arrow(t) && static_cast<int>(t) >= 51);
}

struct text_dims {
    double width = 0, height = 0;
};

class Drawing {
public:
    Drawing(const Model& model, const RenderOptions& options)
        : model_(model),
          conf_(model.config),
          options_(options),
          theme_(detail::theme_variables::resolved(init_config(model), options.theme)) {}

    std::string render() {
        check_supported();
        layout();
        return draw();
    }

private:
    struct actor_box {
        const Actor* actor = nullptr;
        double width = 0, height = 0, margin = 0, x = 0, starty = 0, stopy = 0;
        std::string label;  // the description as drawn: wrapped when the actor asks for it
        bool margin_set = false;  // Mermaid sets `actor.margin` only for actors a message or note widens
        size_t index = 0;
        bool visible = true;    // hideUnusedParticipants takes the actors no statement names out of the drawing
        size_t draw_index = 0;  // position among the visible actors (palette colour, element ids)
        double center() const { return x + width / 2; }
    };

    // a `box` around actors (Mermaid's boxModel)
    struct group_box {
        const Box* box = nullptr;
        std::string text;  // the title as drawn: wrapped when the box asks for it
        double x = 0, y = 0, width = 0, margin = 0;
        double startx = 0, starty = 0, stopx = 0, stopy = 0;  // what is drawn
        bool used = false;  // some actor of the box is drawn
    };

    struct message_box {
        const Message* message = nullptr;
        double startx = 0, stopx = 0, starty = 0, stopy = 0, line_y = 0, from_bounds = 0, to_bounds = 0, width = 0;
        std::string text;       // the text as drawn: wrapped when the message asks for it
        double number = 0;      // the `autonumber` index of the message
        bool numbered = false;  // drawn with the number circle
        bool autonumbered = false;  // numbered by an `autonumber` statement (msgModel.sequenceVisible), not by the config key
    };

    struct note_box {
        const Message* message = nullptr;
        std::string text;  // as drawn: wrapped when the note asks for it
        double startx = 0, width = 0;
    };

    // One entry of bounds.activations: the model phase and the drawing phase each keep their own stack.
    struct activation {
        double startx = 0, stopx = 0, starty = 0;
        std::string actor;
        size_t slot = 0;  // where its rectangle goes among the elements that precede the messages
    };

    // bounds.sequenceItems: a fragment being drawn (`loop`, `alt`, ... and the background of `rect`)
    struct loop_item {
        std::optional<double> startx, stopx, starty, stopy;
        std::string title;
        bool wrap = false;
        std::vector<double> sections;               // y of every `else` / `and` / `option`
        std::vector<std::string> section_titles;
        std::string fill;                           // `rect`
    };

    static flowchart::InitConfig init_config(const Model& model) {
        flowchart::InitConfig config;
        config.theme = model.config.theme;
        config.font_family = model.config.font_family;
        config.theme_variables = model.config.theme_variables;
        config.look = model.config.look;
        return config;
    }

    // control, entity and database: a glyph in the 44px band above the label (neo look only for now)
    // a control, entity or boundary of the classic look: drawActorTypeControl/Entity/Boundary measure the glyph with getBBox() and set the actor's
    // height to it plus the label box height (twice for a control); the top copy is drawn with the height it had, the mirrored copy with the new one
    bool classic_glyph(const actor_box& a) const {
        return !conf_.neo && (a.actor->type == "control" || a.actor->type == "entity" || a.actor->type == "boundary");
    }
    double classic_glyph_height(const actor_box& a) const { return 44 + (a.actor->type == "control" ? 2 : 1) * conf_.label_box_height; }

    static bool is_glyph_type(const std::string& t) { return t == "control" || t == "entity" || t == "database" || t == "boundary"; }

    void check_supported() const {
        const auto unsupported = [](const std::string& what) { throw UnsupportedDiagram("sequence diagrams: " + what + " is not drawn yet"); };
        if (const std::string look = model_.config.look.value_or("neo"); look != "neo" && look != "classic") unsupported("look `" + look + "`");
        for (const Actor& a : model_.actors) {
            for (const auto& [key, value] : a.properties)
                if (key == "icon") unsupported("an actor `icon` property");
            if (a.type != "actor" && a.type != "participant" && a.type != "collections" && a.type != "queue" && a.type != "control" && a.type != "entity" && a.type != "database" && a.type != "boundary") unsupported("the participant type `" + a.type + "`");
            // drawPopup reads the rect of the box, which a figure or glyph never gets: Mermaid itself fails on them
            if ((a.type == "actor" || a.type == "control" || a.type == "entity" || a.type == "boundary") && !a.links.empty()) unsupported("a menu on a `" + a.type + "` (Mermaid cannot draw it)");
            // the classic database cylinder is sized from the float32 bounds of its path, which depends on the actor width
            if (a.type == "database" && !conf_.neo) unsupported("the participant type `database` in the classic look");
        }
        for (const std::string& key : model_.config.unsupported) unsupported("the config key `" + key + "`");
        // the text model is verified for whole pixel sizes from 6 to 48
        if (conf_.font_size < 6 || conf_.font_size > 48 || conf_.font_size != std::floor(conf_.font_size)) unsupported("a `fontSize` of " + num(conf_.font_size));
        for (const Message& m : model_.messages) {
            if (!m.from.empty() && !model_.find_actor(m.from))
                throw ParseError("`" + m.from + "` is not a participant of the diagram", 0);
            const bool drawn = is_arrow(m.type) || m.type == LineType::Note || is_activation_start(m.type) || m.type == LineType::ActiveEnd;
            const bool fragment = is_fragment_start(m.type) || is_fragment_end(m.type) || m.type == LineType::AltElse || m.type == LineType::ParAnd ||
                                  m.type == LineType::CriticalOption || m.type == LineType::RectStart || m.type == LineType::RectEnd;
            if (!drawn && !fragment && m.type != LineType::Autonumber) unsupported(std::string("a statement of type ") + std::to_string(static_cast<int>(m.type)));
        }
    }

    // utils.calculateTextDimensions (see svg_text_metrics.h): whole pixels, the face Mermaid ends up measuring with.
    // setConf copies the top-level `fontSize` over actorFontSize, noteFontSize and messageFontSize, so one size draws every text
    std::string font_style() const { return "font-size: " + num(conf_.font_size) + "px; font-weight: 400;"; }
    text_dims measure(const std::string& text) const {
        const detail::svg_text_dimensions d = detail::svg_text_metrics::calculate(text, conf_.font_size);
        return {d.width, d.height};
    }

    void update_val(std::optional<double>& value, double v, bool take_min) {
        if (!value) value = v;
        else value = take_min ? MERMAID_MIN(v, *value) : MERMAID_MAX(v, *value);
    }

    // bounds.insert + updateBounds: every fragment still open grows by `n` boxMargins, n counting down from the outermost;
    // the bars still open are counted after the fragments and so get n <= 0
    void insert(double sx, double sy, double ex, double ey) {
        const double x0 = MERMAID_MIN(sx, ex), x1 = MERMAID_MAX(sx, ex), y0 = MERMAID_MIN(sy, ey), y1 = MERMAID_MAX(sy, ey);
        data_startx_ = has_bounds_ ? MERMAID_MIN(data_startx_, x0) : x0;
        data_starty_ = has_bounds_ ? MERMAID_MIN(data_starty_, y0) : y0;
        data_stopx_ = has_bounds_ ? MERMAID_MAX(data_stopx_, x1) : x1;
        data_stopy_ = has_bounds_ ? MERMAID_MAX(data_stopy_, y1) : y1;
        has_bounds_ = true;
        long count = 0;
        const long items = static_cast<long>(items_.size());
        for (loop_item& item : items_) {
            ++count;
            const double n = static_cast<double>(items - count + 1) * conf_.box_margin;
            update_val(item.starty, y0 - n, true);
            update_val(item.stopy, y1 + n, false);
            data_startx_ = MERMAID_MIN(data_startx_, x0 - n);
            data_stopx_ = MERMAID_MAX(data_stopx_, x1 + n);
            update_val(item.startx, x0 - n, true);
            update_val(item.stopx, x1 + n, false);
            data_starty_ = MERMAID_MIN(data_starty_, y0 - n);
            data_stopy_ = MERMAID_MAX(data_stopy_, y1 + n);
        }
        for (activation& act : open_) {
            ++count;
            const double n = static_cast<double>(items - count + 1) * conf_.box_margin;
            act.starty = MERMAID_MIN(act.starty, y0 - n);
            data_startx_ = MERMAID_MIN(data_startx_, x0 - n);
            data_stopx_ = MERMAID_MAX(data_stopx_, x1 + n);
        }
    }

    void bump(double amount) {
        vertical_ += amount;
        data_stopy_ = has_bounds_ ? MERMAID_MAX(vertical_, data_stopy_) : vertical_;
        has_bounds_ = true;
    }

    const actor_box* find_actor(const std::string& key) const {
        for (const actor_box& a : actors_)
            if (a.actor->key == key) return &a;
        return nullptr;
    }

    actor_box* find(const std::string& key) {
        for (actor_box& a : actors_)
            if (a.actor->key == key) return &a;
        return nullptr;
    }

    static size_t count_of(const std::vector<activation>& stack, const std::string& actor) {
        return static_cast<size_t>(std::count_if(stack.begin(), stack.end(), [&](const activation& a) { return a.actor == actor; }));
    }

    // activationBounds: the lifeline's own 2px, widened by the bars on it
    std::pair<double, double> activation_bounds(const actor_box& a, const std::vector<activation>& stack) const {
        double left = a.center() - 1, right = a.center() + 1;
        for (const activation& act : stack)
            if (act.actor == a.actor->key) {
                left = MERMAID_MIN(left, act.startx);
                right = MERMAID_MAX(right, act.stopx);
            }
        return {left, right};
    }

    // buildMessageModel
    message_box build_message(const Message& m, const std::vector<activation>& stack) {
        message_box box;
        box.message = &m;
        const actor_box& from = *find(m.from);
        const actor_box& to = *find(m.to);
        const auto [from_left, from_right] = activation_bounds(from, stack);
        const auto [to_left, to_right] = activation_bounds(to, stack);
        const bool to_right_side = from_left <= to_left;
        double startx = to_right_side ? from_right : from_left;
        double stopx = to_right_side ? to_left : to_right;
        const bool bidirectional = m.type == LineType::BidirectionalSolid || m.type == LineType::BidirectionalDotted;
        if (conf_.neo && m.type != LineType::SolidOpen) stopx += to_right_side ? -kNeoOffset : kNeoOffset;
        if (conf_.neo && bidirectional) startx += to_right_side ? kNeoOffset : -kNeoOffset;
        if (m.central == static_cast<int>(LineType::CentralConnectionReverse) || m.central == static_cast<int>(LineType::CentralConnectionDual)) {
            startx += 4;  // CENTRAL_CONNECTION_BASE_OFFSET
            if (bidirectional) startx += to_right_side ? 0 : -6;
        }
        const auto adjust = [&](double v) { return to_right_side ? -v : v; };
        if (m.from == m.to) {
            stopx = startx;
        } else {
            const bool arrow_to_activation = std::abs(to_left - to_right) > 2;
            if (m.activate && !arrow_to_activation) stopx += adjust(conf_.activation_width / 2 - 1);
            if (m.type != LineType::SolidOpen && m.type != LineType::DottedOpen && !is_stick_arrow(m.type) && !is_reverse_arrow(m.type)) stopx += adjust(3);
            if (bidirectional || is_solid_reverse_arrow(m.type)) startx -= adjust(3);
        }
        box.startx = startx;
        box.stopx = stopx;
        const double bounded = std::abs(startx - stopx);
        // a wrapped message is wrapped to the width its actors leave and then no longer widens the gap by its text
        box.text = m.wrap && !m.text.empty() ? wrap_label(m.text, MERMAID_MAX(bounded + 2 * conf_.wrap_padding, conf_.width)) : m.text;
        box.width = MERMAID_MAX(MERMAID_MAX(m.wrap ? 0 : measure(box.text).width + 2 * conf_.wrap_padding, bounded + 2 * conf_.wrap_padding), conf_.width);
        box.from_bounds = MERMAID_MIN(MERMAID_MIN(from_left, from_right), MERMAID_MIN(to_left, to_right));
        box.to_bounds = MERMAID_MAX(MERMAID_MAX(from_left, from_right), MERMAID_MAX(to_left, to_right));
        return box;
    }

    // buildNoteModel
    note_box build_note(const Message& m) {
        const actor_box& from = *find(m.from);
        const actor_box& to = *find(m.to);
        const bool wrap = m.wrap && !m.text.empty();
        text_dims d = measure(wrap ? wrap_label(m.text, conf_.width) : m.text);
        note_box note;
        note.message = &m;
        note.width = wrap ? conf_.width : MERMAID_MAX(conf_.width, d.width + 2 * conf_.note_margin);
        note.startx = from.x;
        if (*m.placement == Placement::RightOf) {
            note.width = wrap ? MERMAID_MAX(conf_.width, d.width) : MERMAID_MAX(from.width / 2 + to.width / 2, d.width + 2 * conf_.note_margin);
            note.startx = from.x + (from.width + conf_.actor_margin) / 2;
        } else if (*m.placement == Placement::LeftOf) {
            note.width = wrap ? MERMAID_MAX(conf_.width, d.width + 2 * conf_.note_margin)
                              : MERMAID_MAX(from.width / 2 + to.width / 2, d.width + 2 * conf_.note_margin);
            note.startx = from.x - note.width + (from.width - conf_.actor_margin) / 2;
        } else if (m.to == m.from) {
            d = measure(wrap ? wrap_label(m.text, MERMAID_MAX(conf_.width, from.width)) : m.text);
            note.width = wrap ? MERMAID_MAX(conf_.width, from.width)
                              : MERMAID_MAX(MERMAID_MAX(from.width, conf_.width), d.width + 2 * conf_.note_margin);
            note.startx = from.x + (from.width - note.width) / 2;
        } else {
            note.width = std::abs(from.x + from.width / 2 - (to.x + to.width / 2)) + conf_.actor_margin;
            note.startx = from.x < to.x ? from.x + from.width / 2 - conf_.actor_margin / 2 : to.x + to.width / 2 - conf_.actor_margin / 2;
        }
        note.text = wrap ? wrap_label(m.text, note.width - 2 * conf_.wrap_padding) : m.text;
        return note;
    }

    void layout() {
        for (size_t i = 0; i < model_.actors.size(); ++i) {
            actor_box a;
            a.actor = &model_.actors[i];
            a.index = i;
            a.margin = conf_.actor_margin;
            a.label = a.actor->wrap ? wrap_label(a.actor->description, conf_.width - 2 * conf_.wrap_padding) : a.actor->description;
            const text_dims d = measure(a.label);
            a.width = a.actor->wrap ? conf_.width : MERMAID_MAX(conf_.width, d.width + 2 * conf_.wrap_padding);
            if (conf_.neo)
                a.height = MERMAID_MAX(kGlyphBand + kGlyphGap + d.height + kLifelineGap, conf_.height);
            else
                a.height = a.actor->wrap ? MERMAID_MAX(d.height, conf_.height) : conf_.height;
            actors_.push_back(a);
        }
        // getMaxMessageWidthPerActor: which gaps a message or a note widens
        std::map<std::string, double> widest;
        const auto widen = [&](const std::string& key, double w) { widest[key] = MERMAID_MAX(widest[key], w); };
        for (const Message& m : model_.messages) {
            actor_box* to = find(m.to);
            if (!find(m.from) || !to) continue;
            const bool has_prev = to->index > 0, has_next = to->index + 1 < actors_.size();
            if (m.placement == Placement::LeftOf && !has_prev) continue;
            if (m.placement == Placement::RightOf && !has_next) continue;
            const double w = measure(m.wrap ? wrap_label(m.text, conf_.width - 2 * conf_.wrap_padding) : m.text).width + 2 * conf_.wrap_padding;
            const std::string prev_key = has_prev ? actors_[to->index - 1].actor->key : "";
            if (!m.placement) {
                const bool from_is_next = has_next && actors_[to->index + 1].actor->key == m.from;
                const bool from_is_prev = has_prev && prev_key == m.from;
                if (from_is_next) widen(m.to, w);
                else if (from_is_prev) widen(m.from, w);
                else if (m.from == m.to) {
                    widen(m.from, w / 2);
                    widen(m.to, w / 2);
                }
            } else if (*m.placement == Placement::RightOf) {
                widen(m.from, w);
            } else if (*m.placement == Placement::LeftOf) {
                widen(prev_key, w);
            } else if (*m.placement == Placement::Over) {
                if (has_prev) widen(prev_key, w / 2);
                if (has_next) widen(m.from, w / 2);
            }
        }
        // calculateActorMargins
        for (actor_box& a : actors_) {
            const auto it = widest.find(a.actor->key);
            if (it == widest.end()) continue;
            const bool last = a.index + 1 >= actors_.size();
            const double next_half = last ? 0 : actors_[a.index + 1].width / 2;
            a.margin = MERMAID_MAX(it->second + conf_.actor_margin - a.width / 2 - next_half, conf_.actor_margin);
            a.margin_set = true;
        }
        double height = 0;
        for (const actor_box& a : actors_) height = MERMAID_MAX(height, a.height);
        actor_height_ = MERMAID_MAX(height, conf_.height);
        // calculateActorMargins, boxes: room for the actors of a box, its title, and the margin the title needs beyond that
        double box_text_height = 0;
        for (const Box& box : model_.boxes) {
            group_box g;
            g.box = &box;
            double total = 0;
            for (const std::string& key : box.actor_keys) {
                const actor_box& a = *find(key);
                total += a.width + (a.margin_set ? a.margin : 0);
            }
            total += conf_.box_margin * 8 - 2 * conf_.box_text_margin;
            g.text = box.wrap ? wrap_label(box.name, total - 2 * conf_.wrap_padding) : box.name;
            const text_dims d = measure(g.text);
            box_text_height = MERMAID_MAX(box_text_height, d.height);
            const double min_width = MERMAID_MAX(total, d.width + 2 * conf_.wrap_padding);
            g.margin = conf_.box_text_margin;
            if (total < min_width) g.margin += (min_width - total) / 2;
            boxes_.push_back(g);
        }
        box_text_height_ = box_text_height;
        if (!model_.boxes.empty()) {
            bump(conf_.box_margin);
            for (const Box& box : model_.boxes)
                if (!box.name.empty()) {  // hasAtLeastOneBoxWithTitle: the titles sit above the actors
                    bump(box_text_height_);
                    break;
                }
        }
        // hideUnusedParticipants: only the actors some statement names are placed and drawn (the widths above counted them all)
        if (conf_.hide_unused_participants) {
            std::set<std::string> named;
            for (const Message& m : model_.messages) {
                named.insert(m.from);
                named.insert(m.to);
            }
            for (actor_box& a : actors_) a.visible = named.count(a.actor->key) != 0;
        }
        for (actor_box& a : actors_)
            if (a.visible) a.draw_index = visible_count_++;
        // addActorRenderingData
        double prev_width = 0, prev_margin = 0;
        int prev_box = -1;
        for (actor_box& a : actors_) {
            if (!a.visible) continue;
            if (a.actor->box >= 0) boxes_[static_cast<size_t>(a.actor->box)].used = true;
            a.height = MERMAID_MAX(a.height, actor_height_);
            const int box = a.actor->box;
            if (prev_box >= 0 && prev_box != box) prev_margin += conf_.box_margin + boxes_[static_cast<size_t>(prev_box)].margin;
            if (box >= 0 && box != prev_box) {
                boxes_[static_cast<size_t>(box)].x = prev_width + prev_margin;
                boxes_[static_cast<size_t>(box)].y = 0;
                prev_margin += boxes_[static_cast<size_t>(box)].margin;
            }
            // a created actor leaves room for its half width (Mermaid tests the index for truth: a `create` before the first message is 0)
            const auto created = model_.created_actors.find(a.actor->key);
            if (created != model_.created_actors.end() && created->second != 0) prev_margin += a.width / 2;
            a.x = prev_width + prev_margin;
            a.starty = vertical_;
            insert(a.x, 0, a.x + a.width, a.height);
            prev_width += a.width + prev_margin;
            if (box >= 0) boxes_[static_cast<size_t>(box)].width = prev_width + boxes_[static_cast<size_t>(box)].margin - boxes_[static_cast<size_t>(box)].x;
            prev_margin = a.margin;
            prev_box = box;
        }
        bump(actor_height_);
        // the models of every statement, with the activations as they are at that point (calculateLoopBounds)
        struct model_entry {
            std::optional<message_box> message;
            std::optional<note_box> note;
        };
        std::vector<model_entry> models(model_.messages.size());
        std::vector<activation> stack;
        struct frame {
            std::string id;
            size_t slot;
            double from, to;
        };
        std::vector<frame> frames;
        for (size_t i = 0; i < model_.messages.size(); ++i) {
            const Message& m = model_.messages[i];
            if (is_fragment_start(m.type)) {
                frames.push_back({m.id, loop_width_.size(), 1e300, -1e300});
                loop_width_.push_back(0);
            } else if (m.type == LineType::AltElse || m.type == LineType::ParAnd || m.type == LineType::CriticalOption) {
                if (!m.text.empty() && !frames.empty()) {
                    loop_slot_[frames.back().id] = frames.back().slot;
                    loop_slot_[m.id] = frames.back().slot;
                }
            } else if (is_fragment_end(m.type) && !frames.empty()) {
                loop_slot_[frames.back().id] = frames.back().slot;
                frames.pop_back();
            }
            if (m.type == LineType::ActiveStart) {
                const actor_box& a = *find(m.from);
                activation act;
                act.startx = a.center() + (static_cast<double>(count_of(stack, m.from)) - 1) * conf_.activation_width / 2;
                act.stopx = act.startx + conf_.activation_width;
                act.actor = m.from;
                stack.push_back(act);
            } else if (m.type == LineType::ActiveEnd) {
                for (size_t k = stack.size(); k-- > 0;)
                    if (stack[k].actor == m.from) {
                        stack.erase(stack.begin() + static_cast<std::ptrdiff_t>(k));
                        break;
                    }
            } else if (m.type == LineType::Note) {
                models[i].note = build_note(m);
                for (frame& f : frames) {
                    f.from = MERMAID_MIN(f.from, models[i].note->startx);
                    f.to = MERMAID_MAX(f.to, models[i].note->startx + models[i].note->width);
                    loop_width_[f.slot] = MERMAID_MAX(loop_width_[f.slot], std::abs(f.from - f.to)) - conf_.label_box_width;
                }
            } else if (is_arrow(m.type)) {
                models[i].message = build_message(m, stack);
                const message_box& box = *models[i].message;
                if (box.startx != 0 && box.stopx != 0)
                    for (frame& f : frames) {
                        if (box.startx == box.stopx) {
                            const actor_box& from = *find(m.from);
                            const actor_box& to = *find(m.to);
                            f.from = MERMAID_MIN(MERMAID_MIN(from.x - box.width / 2, from.x - from.width / 2), f.from);
                            f.to = MERMAID_MAX(MERMAID_MAX(to.x + box.width / 2, to.x + from.width / 2), f.to);
                            loop_width_[f.slot] = MERMAID_MAX(loop_width_[f.slot], std::abs(f.to - f.from)) - conf_.label_box_width;
                        } else {
                            f.from = MERMAID_MIN(box.startx, f.from);
                            f.to = MERMAID_MAX(box.stopx, f.to);
                            loop_width_[f.slot] = MERMAID_MAX(loop_width_[f.slot], box.width) - conf_.label_box_width;
                        }
                    }
            }
        }
        // the drawing loop
        for (size_t i = 0; i < model_.messages.size(); ++i) {
            const Message& m = model_.messages[i];
            if (m.type == LineType::Note) {
                const note_box& n = *models[i].note;
                bump(conf_.box_margin);
                const double starty = vertical_;
                const std::vector<std::string> rows = detail::split_breaks(n.text);
                double text_height = 0;
                for (const std::string& row : rows) text_height += detail::svg_text_metrics::drawn_height(detail::decode_entity_codes(row), conf_.font_size);
                bump(text_height + 2 * conf_.note_margin);
                insert(n.startx, starty, n.startx + n.width, starty + text_height + 2 * conf_.note_margin);
                write_note(n, starty, text_height, rows);
            } else if (is_activation_start(m.type)) {
                const actor_box& a = *find(m.from);
                activation act;
                act.startx = a.center() + (static_cast<double>(count_of(open_, m.from)) - 1) * conf_.activation_width / 2;
                act.stopx = act.startx + conf_.activation_width;
                act.starty = vertical_ + 2;
                act.actor = m.from;
                act.slot = pre_.size();
                pre_.push_back("<g></g>");  // the anchor: the rectangle is put in when the activation ends
                open_.push_back(act);
            } else if (m.type == LineType::ActiveEnd) {
                for (size_t k = open_.size(); k-- > 0;) {
                    if (open_[k].actor != m.from) continue;
                    activation act = open_[k];
                    open_.erase(open_.begin() + static_cast<std::ptrdiff_t>(k));
                    double v = vertical_;
                    if (act.starty + 18 > v) {
                        act.starty = v - 6;
                        v += 12;
                    }
                    write_activation(act, v, count_of(open_, m.from));
                    insert(act.startx, v - 10, act.stopx, v);
                    break;
                }
            } else if (m.type == LineType::Autonumber) {
                if (m.number_start.value_or(0) != 0) number_ = *m.number_start;  // JavaScript: `start || index`
                if (m.number_step.value_or(0) != 0) number_step_ = *m.number_step;
                numbers_on_ = m.number_visible.value_or(false);
            } else if (is_fragment_start(m.type) || m.type == LineType::RectStart) {
                const bool rect = m.type == LineType::RectStart;
                adjust_for_wrap(m, conf_.box_margin, rect ? conf_.box_margin : conf_.box_margin + conf_.box_text_margin, [&](const std::string& title, bool wrap) {
                    loop_item item;
                    item.starty = vertical_;
                    item.title = title;
                    item.wrap = wrap;
                    if (rect) item.fill = m.text.empty() ? "hsl(0, 0%, 96%)" : m.text;  // without a colour: the default theme's light grey
                    items_.push_back(std::move(item));
                });
            } else if (m.type == LineType::AltElse || m.type == LineType::ParAnd || m.type == LineType::CriticalOption) {
                adjust_for_wrap(m, conf_.box_margin + conf_.box_text_margin, conf_.box_margin, [&](const std::string& title, bool) {
                    items_.back().sections.push_back(vertical_);
                    items_.back().section_titles.push_back(title);
                });
            } else if (is_fragment_end(m.type) || m.type == LineType::RectEnd) {
                loop_item item = std::move(items_.back());
                items_.pop_back();
                if (m.type == LineType::RectEnd) backgrounds_.push_back(item);
                else write_loop(item, fragment_word(m.type), m);
                bump(item.stopy.value_or(vertical_) - vertical_);
            } else {
                message_box box = *models[i].message;
                box.number = number_;
                box.numbered = numbers_on_ || conf_.show_sequence_numbers;
                box.autonumbered = numbers_on_;
                number_ = std::floor((number_ + number_step_) * 100 + 0.5) / 100;
                box.starty = vertical_;
                bump(10);
                const text_dims d = measure(box.text);
                const double lines = static_cast<double>(detail::split_breaks(box.text).size());
                double height = 0;
                height += d.height / lines;
                bump(d.height / lines);
                double total_offset = d.height - 10;
                if (box.startx == box.stopx) {  // a message to oneself loops out to the right and back
                    box.line_y = vertical_ + total_offset;
                    if (!conf_.right_angles) {
                        total_offset += conf_.box_margin;
                        box.line_y = vertical_ + total_offset;
                    }
                    total_offset += 30;
                    const double dx = MERMAID_MAX(d.width / 2, conf_.width / 2);
                    insert(box.startx - dx, vertical_ - 10 + total_offset, box.stopx + dx, vertical_ + 30 + total_offset);
                } else {
                    total_offset += conf_.box_margin;
                    box.line_y = vertical_ + total_offset;
                    insert(box.startx, box.line_y - 10, box.stopx, box.line_y);
                }
                bump(total_offset);
                height += total_offset;
                box.stopy = box.starty + height;
                insert(box.from_bounds, box.starty, box.to_bounds, box.stopy);
                adjust_created_destroyed(m, box, i);
                messages_.push_back(box);
            }
        }
        // the mirrored actors
        if (conf_.mirror_actors) {
            bump(conf_.box_margin * 2);
            for (actor_box& a : actors_)
                if (a.stopy == 0) a.stopy = vertical_;  // a destroyed actor was given its own
            // drawActors returns the height each mirrored actor ended up with
            double footer_height = actor_height_;
            for (const actor_box& a : actors_)
                if (a.visible && classic_glyph(a)) {
                    footer_height = 0;
                    break;
                }
            if (footer_height == 0)
                for (const actor_box& a : actors_)
                    if (a.visible) footer_height = MERMAID_MAX(footer_height, classic_glyph(a) ? classic_glyph_height(a) : a.height);
            bump(footer_height + conf_.box_margin);
        }
        // the boxes close at the bottom of the diagram
        for (group_box& g : boxes_) {
            if (!g.used) continue;
            const double height = vertical_ - g.y;
            insert(g.x, g.y, g.x + g.width, height);
            const double padding = conf_.box_margin * 2;
            g.startx = g.x - padding;
            g.starty = g.y - padding * 0.25;
            g.stopx = g.startx + g.width + 2 * padding;
            g.stopy = g.starty + height + padding * 0.75;
        }
        if (!boxes_.empty()) bump(conf_.box_margin);
    }

    // adjustCreatedDestroyedData: the message to a created actor (or from or to a destroyed one) stops at its box, and the box
    // is centred on the message line
    void adjust_created_destroyed(const Message& m, message_box& box, size_t index) {
        constexpr double kActorTypeWidth = 36;  // a stick figure; Mermaid sizes the arrow to it for a control, entity and database too (not a boundary)
        actor_box* from = find(m.from);
        actor_box* to = find(m.to);
        const auto is_man = [](const actor_box& a) { const std::string& t = a.actor->type; return t == "actor" || t == "control" || t == "entity" || t == "database"; };
        const auto receiver = [&](const actor_box& actor, double adjustment) {
            if (actor.x < from->x) {
                insert(box.stopx - adjustment, box.starty, box.startx, box.stopy + actor.height / 2 + conf_.note_margin);
                box.stopx += adjustment;
            } else {
                insert(box.startx, box.starty, box.stopx + adjustment, box.stopy + actor.height / 2 + conf_.note_margin);
                box.stopx -= adjustment;
            }
        };
        const auto sender = [&](const actor_box& actor, double adjustment) {
            if (actor.x < to->x) {
                insert(box.startx - adjustment, box.starty, box.stopx, box.stopy + actor.height / 2 + conf_.note_margin);
                box.startx += adjustment;
            } else {
                insert(box.stopx, box.starty, box.startx + adjustment, box.stopy + actor.height / 2 + conf_.note_margin);
                box.startx -= adjustment;
            }
        };
        const auto at = [&](const std::map<std::string, size_t>& indices, const std::string& key) {
            const auto it = indices.find(key);
            return it != indices.end() && it->second == index;
        };
        if (at(model_.created_actors, m.to)) {
            receiver(*to, is_man(*to) ? kActorTypeWidth / 2 + 3 : to->width / 2 + 3);
            to->starty = box.line_y - to->height / 2;
            bump(to->height / 2);
        } else if (at(model_.destroyed_actors, m.from)) {
            if (conf_.mirror_actors) sender(*from, is_man(*from) ? kActorTypeWidth / 2 : from->width / 2);
            from->stopy = box.line_y - from->height / 2;
            bump(from->height / 2);
        } else if (at(model_.destroyed_actors, m.to)) {
            if (conf_.mirror_actors) receiver(*to, is_man(*to) ? kActorTypeWidth / 2 + 3 : to->width / 2 + 3);
            to->stopy = box.line_y - to->height / 2;
            bump(to->height / 2);
        }
    }

    static bool is_fragment_start(LineType t) {
        return t == LineType::LoopStart || t == LineType::AltStart || t == LineType::OptStart || t == LineType::ParStart ||
               t == LineType::ParOverStart || t == LineType::CriticalStart || t == LineType::BreakStart;
    }
    static bool is_fragment_end(LineType t) {
        return t == LineType::LoopEnd || t == LineType::AltEnd || t == LineType::OptEnd || t == LineType::ParEnd ||
               t == LineType::CriticalEnd || t == LineType::BreakEnd;
    }
    static const char* fragment_word(LineType end) {
        switch (end) {
            case LineType::LoopEnd: return "loop";
            case LineType::AltEnd: return "alt";
            case LineType::OptEnd: return "opt";
            case LineType::ParEnd: return "par";
            case LineType::CriticalEnd: return "critical";
            default: return "break";
        }
    }

    // utils.breakString: characters fill a line until it reaches `max_width`; a full line ends with a hyphen unless it ends the word
    std::vector<std::string> break_string(const std::string& word, double max_width, std::string& remaining) const {
        std::vector<std::string> characters;
        for (size_t i = 0; i < word.size();) {
            size_t length = 1;
            while (i + length < word.size() && (static_cast<unsigned char>(word[i + length]) & 0xC0) == 0x80) ++length;
            characters.push_back(word.substr(i, length));
            i += length;
        }
        std::vector<std::string> lines;
        std::string current;
        for (size_t i = 0; i < characters.size(); ++i) {
            const std::string next = current + characters[i];
            if (measure(next).width >= max_width) {
                lines.push_back(i + 1 == characters.size() ? next : next + "-");
                current.clear();
            } else {
                current = next;
            }
        }
        remaining = current;
        return lines;
    }

    // utils.wrapLabel: words fill a line until the next one would reach `max_width`
    std::string wrap_label(const std::string& label, double max_width) const {
        if (label.empty() || detail::split_breaks(label).size() > 1) return label;
        std::vector<std::string> words;
        for (size_t pos = 0; pos <= label.size();) {
            const size_t space = label.find(' ', pos);
            std::string word = label.substr(pos, space == std::string::npos ? std::string::npos : space - pos);
            pos = space == std::string::npos ? label.size() + 1 : space + 1;
            if (!word.empty()) words.push_back(std::move(word));
        }
        std::vector<std::string> lines;
        std::string next;
        for (size_t i = 0; i < words.size(); ++i) {
            const double word_length = measure(words[i] + " ").width;
            const double next_length = next.empty() ? 0.0 : measure(next).width;
            if (word_length > max_width) {  // a word wider than the line is cut and hyphenated (breakString)
                lines.push_back(next);
                std::string rest;
                for (const std::string& line : break_string(words[i], max_width, rest)) lines.push_back(line);
                next = rest;
            } else if (next_length + word_length >= max_width) {
                lines.push_back(next);
                next = words[i];
            } else {
                next = next.empty() ? words[i] : next + " " + words[i];
            }
            if (i + 1 == words.size()) lines.push_back(next);
        }
        std::string out;
        for (const std::string& line : lines)
            if (!line.empty()) out += (out.empty() ? "" : "<br/>") + line;
        return out;
    }

    // adjustLoopHeightForWrap: room above, the title (wrapped to the width the fragment turned out to need), room below
    template <typename Add>
    void adjust_for_wrap(const Message& m, double pre_margin, double post_margin, Add add) {
        bump(pre_margin);
        double height_adjust = post_margin;
        std::string title = m.text;
        bool wrap = false;
        const auto slot = loop_slot_.find(m.id);
        if (!m.id.empty() && !m.text.empty() && slot != loop_slot_.end()) {
            const double width = loop_width_[slot->second];
            title = wrap_label("[" + m.text + "]", width - 2 * conf_.wrap_padding);
            wrap = true;
            height_adjust = post_margin + MERMAID_MAX(measure(title).height, conf_.label_box_height);
        }
        add(title, wrap);
        bump(height_adjust);
    }

    // drawLoop
    void write_loop(const loop_item& item, const char* word, const Message& end) {
        SvgWriter out;
        out.open("g", {{"data-et", "control-structure"}, {"data-id", "i" + end.id}});
        const double sx = item.startx.value_or(0), sy = item.starty.value_or(0), ex = item.stopx.value_or(0), ey = item.stopy.value_or(0);
        const auto line = [&](double x1, double y1, double x2, double y2, bool dashed) {
            SvgWriter::Attrs a{{"x1", num(x1)}, {"y1", num(y1)}, {"x2", num(x2)}, {"y2", num(y2)}, {"class", "loopLine"}};
            if (dashed) a.emplace_back("style", "stroke-dasharray: 3, 3;");
            out.leaf("line", a);
        };
        line(sx, sy, ex, sy, false);
        line(ex, sy, ex, ey, false);
        line(sx, ey, ex, ey, false);
        line(sx, sy, sx, ey, false);
        for (double y : item.sections) line(sx, y, ex, y, true);
        // the label box of the neo look: 50 wide, 20 + 15 high, the lower right corner cut by 7
        const double w = conf_.label_box_width, h = conf_.label_box_height + (conf_.neo ? 15 : 0);
        constexpr double cut = 7;
        out.leaf("polygon", {{"points", num(sx) + "," + num(sy) + " " + num(sx + w) + "," + num(sy) + " " + num(sx + w) + "," +
                                           num(sy + h - cut) + " " + num(sx + w - cut * 1.2) + "," + num(sy + h) + " " + num(sx) + "," + num(sy + h)},
                             {"class", "labelBox"}});
        write_text_block(out, {word}, js_round(sx + w / 2), sy + h / 2, "labelText", conf_.box_text_margin, false, true);
        write_text_block(out, detail::split_breaks(item.title), sx + conf_.label_box_width / 2 + (ex - sx) / 2, sy + conf_.box_margin + conf_.box_text_margin, "loopText",
                         conf_.box_text_margin, true, false);
        for (size_t k = 0; k < item.section_titles.size(); ++k)
            if (!item.section_titles[k].empty())
                write_text_block(out, detail::split_breaks(item.section_titles[k]), sx + (ex - sx) / 2, item.sections[k] + conf_.box_margin + conf_.box_text_margin,
                                 "sectionTitle", conf_.box_text_margin, false, false);
        out.close();
        pre_.push_back(out.str());
    }

    // drawText for the texts of fragments: one <text> per row, centred; `middle` adds the baseline attributes
    void write_text_block(SvgWriter& out, const std::vector<std::string>& rows, double x, double y, const std::string& css_class, double margin, bool tspan,
                          bool middle) const {
        double drawn = 0;
        for (const std::string& row : rows) {
            SvgWriter::Attrs attrs{{"x", num(x)}, {"y", num(js_round(y + (2 * drawn + margin) / 2))}, {"text-anchor", "middle"}};
            if (middle) {
                attrs.emplace_back("dominant-baseline", "middle");
                attrs.emplace_back("alignment-baseline", "middle");
            }
            attrs.emplace_back("class", css_class);
            attrs.emplace_back("style", font_style());
            out.open("text", attrs);
            const std::string shown = detail::decode_entity_codes(row);  // measured encoded, drawn as the browser reads it
            const std::string content = shown.empty() ? "\xE2\x80\x8B" : shown;
            if (tspan) out.element("tspan", {{"x", num(x)}}, content);
            else out.text(content);
            out.close();
            drawn += detail::svg_text_metrics::drawn_height(shown, conf_.font_size);
        }
    }

    // drawText: one <text> per row; a row sits half the heights above it plus the margin below the text's top
    // drawText's anchor: where the text sits in the box starting at x0 with width w (left, centre or right, `margin` in from the edge)
    static double aligned_x(const std::string& align, double x0, double w, double margin, std::string& anchor) {
        if (align == "left" || align == "start") {
            anchor = "start";
            return js_round(x0 + margin);
        }
        if (align == "right" || align == "end") {
            anchor = "end";
            return js_round(x0 + w - margin);
        }
        anchor = "middle";
        return js_round(x0 + w / 2);
    }

    void write_rows(SvgWriter& out, const std::vector<std::string>& rows, double x, double y, const std::string& css_class, double margin,
                    bool tspan, const std::string& anchor = "middle") const {
        double drawn = 0;
        for (const std::string& row : rows) {
            const double row_y = js_round(y + (2 * drawn + margin) / 2);
            const std::string shown = detail::decode_entity_codes(row);  // measured encoded, drawn as the browser reads it
            const std::string content = shown.empty() ? "\xE2\x80\x8B" : shown;
            out.open("text", {{"x", num(x)}, {"y", num(row_y)}, {"text-anchor", anchor}, {"dominant-baseline", "middle"},
                              {"alignment-baseline", "middle"}, {"class", css_class}, {"dy", "1em"},
                              {"style", font_style()}});
            if (tspan) out.element("tspan", {{"x", num(x)}}, content);
            else out.text(content);
            out.close();
            drawn += detail::svg_text_metrics::drawn_height(shown, conf_.font_size);
        }
    }

    void write_note(const note_box& n, double starty, double text_height, const std::vector<std::string>& rows) {
        SvgWriter out;
        out.open("g", {{"data-et", "note"}, {"data-id", "i" + n.message->id}});
        SvgWriter::Attrs rect{{"x", num(n.startx)}, {"y", num(starty)}, {"fill", "#EDF2AE"}, {"stroke", "#666"}, {"width", num(n.width)},
                              {"height", num(text_height + 2 * conf_.note_margin)}, {"class", "note"}};
        if (conf_.neo) rect.emplace_back("data-look", "neo");
        out.leaf("rect", rect);
        std::string anchor;
        const double x = aligned_x(conf_.note_align, n.startx, n.width, conf_.note_margin, anchor);
        write_rows(out, rows, x, starty, "noteText", conf_.note_margin, true, anchor);
        out.close();
        pre_.push_back(out.str());
    }

    void write_activation(const activation& act, double vertical, size_t remaining) {
        SvgWriter out;
        out.open("g");
        const actor_box& a = *find(act.actor);
        SvgWriter::Attrs attrs{{"x", num(act.startx)}, {"y", num(act.starty)}, {"fill", "#EDF2AE"}, {"stroke", "#666"},
                               {"width", num(act.stopx - act.startx)}, {"height", num(vertical - act.starty)},
                               {"class", "activation" + std::to_string(remaining % 3)}};
        if (conf_.neo) attrs.emplace_back("data-look", "neo");
        if (const std::string style = actor_style(a); !style.empty()) attrs.emplace_back("style", style);
        out.leaf("rect", attrs);
        out.close();
        pre_[act.slot] = out.str();
    }

    std::string actor_style(const actor_box& a) const {
        const auto& border = theme_.border_color_array;
        const auto& fill = theme_.bkg_color_array;
        if (border.empty() || fill.empty()) return "";
        const auto rgb = [](const std::string& css) {
            const auto c = parse_css_color(css);
            return c ? css_rgb(*c) : css;
        };
        return "stroke: " + rgb(border[a.draw_index % border.size()]) + "; fill: " + rgb(fill[a.draw_index % fill.size()]) + ";";
    }

    // the `class` property replaces `actor` in the class of the box (and the note-yellow fill stays)
    static std::string property_class(const actor_box& a) {
        for (const auto& [key, value] : a.actor->properties)
            if (key == "class") return value;
        return {};
    }
    std::string rect_class(const actor_box& a) const { return property_class(a); }
    std::string rect_class_full(const actor_box& a, bool footer) const {
        const std::string cls = property_class(a);
        return (cls.empty() ? "actor" : cls) + (footer ? " actor-bottom" : " actor-top");
    }

    // drawActorsPopup: the menu of an actor's links hangs under its box; it widens the diagram when it reaches further
    struct popup_size {
        double height = 0, width = 0;
    };
    double popup_min_width(const actor_box& a) const {
        double required = 0;
        for (const auto& link : a.actor->links)
            required = MERMAID_MAX(required, measure(link.first).width + 2 * conf_.wrap_padding + 2 * conf_.box_margin);
        return required;
    }
    static double menu_extent(const actor_box& a) { return 20 + 30 * static_cast<double>(a.actor->links.size()); }
    popup_size popup_extent() const {
        popup_size size;
        for (const actor_box& a : actors_) {
            if (!a.visible || a.actor->links.empty()) continue;
            size.height = MERMAID_MAX(size.height, a.height + menu_extent(a));
            size.width = MERMAID_MAX(size.width, MERMAID_MAX(a.width, popup_min_width(a)) + a.x);
        }
        return size;
    }
    void write_popups(SvgWriter& out) const {
        for (const actor_box& a : actors_) {
            if (!a.visible || a.actor->links.empty()) continue;
            const double menu_width = MERMAID_MAX(a.width, popup_min_width(a));
            out.open("g", {{"id", "actor" + std::to_string(a.draw_index) + "_popup"}, {"class", "actorPopupMenu"}, {"display", "none"}});
            out.leaf("rect", {{"class", "actorPopupMenuPanel " + rect_class_full(a, conf_.mirror_actors)}, {"x", num(a.x)}, {"y", num(a.height)},
                              {"fill", rect_class(a).empty() ? "#eaeaea" : "#EDF2AE"}, {"stroke", "#666"}, {"width", num(menu_width)},
                              {"height", num(menu_extent(a))}, {"rx", conf_.neo ? "6" : "3"}, {"ry", conf_.neo ? "6" : "3"}});
            double link_y = 20;
            for (const auto& [label, url] : a.actor->links) {
                const std::string href = sanitize_url(url);  // sanitizeUrl gives `about:blank`, and the page's own sanitizer drops that href
                out.open("a", href == "about:blank" ? SvgWriter::Attrs{} : SvgWriter::Attrs{{"xlink:href", href}});
                out.open("text", {{"x", num(a.x + 10)}, {"y", num(a.height + link_y + 10)}, {"dominant-baseline", "central"},
                                  {"alignment-baseline", "central"}, {"class", "actor"},
                                  {"style", "text-anchor: start; " + font_style()}});
                out.element("tspan", {{"x", num(a.x + 10)}, {"dy", "0"}}, detail::decode_entity_codes(label));
                out.close();
                out.close();
                link_y += 30;
            }
            out.close();
        }
    }
    // @braintree/sanitize-url: a script-like scheme becomes `about:blank`
    static std::string sanitize_url(const std::string& url) {
        std::string head;
        for (char c : url) {
            if (c == ':' || c == '/' || c == '?' || c == '#') break;
            head += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return head == "javascript" || head == "data" || head == "vbscript" ? "about:blank" : url;
    }

    // an actor's label: one <text> per row, all at the centre, each tspan moved by half the rows' spacing (the font size)
    void write_label_rows(SvgWriter& out, const std::string& label, double x, double y, const char* css_class) const {
        const double size = conf_.font_size;
        const std::vector<std::string> rows = detail::split_breaks(label);
        const double count = static_cast<double>(rows.size());
        for (size_t i = 0; i < rows.size(); ++i) {
            out.open("text", {{"x", num(x)}, {"y", num(y)}, {"dominant-baseline", "central"}, {"alignment-baseline", "central"},
                              {"class", css_class}, {"style", "text-anchor: middle; " + font_style()}});
            out.element("tspan", {{"x", num(x)}, {"dy", num(static_cast<double>(i) * size - (count - 1) * size / 2)}},
                        detail::decode_entity_codes(rows[i]));
            out.close();
        }
    }

    // drawActorTypeActor: a stick figure above its label, scaled into the 44px glyph band of the neo look
    // headerBands / footerBands of the neo look: where the glyph ends and the label is centred
    void neo_bands(const actor_box& a, bool footer, double& glyph_bottom, double& label_center) const {
        const double actor_y = footer ? a.stopy : a.starty;
        const double text_h = measure(a.label).height;
        if (footer) {
            glyph_bottom = actor_y + kLifelineGap + kGlyphBand;
            label_center = glyph_bottom + kGlyphGap + text_h / 2;
        } else {
            const double lifeline_start = actor_y + a.height;
            glyph_bottom = lifeline_start - kLifelineGap - text_h - kGlyphGap;
            label_center = lifeline_start - kLifelineGap - text_h / 2;
        }
    }

    // drawActorTypeControl / drawActorTypeEntity (neo look): a circle with an arrow head or a base line, over the label
    void write_glyph(SvgWriter& out, const actor_box& a, bool footer) const {
        const std::string id = options_.svg_id;
        const bool control = a.actor->type == "control", boundary = a.actor->type == "boundary";
        double glyph_bottom = 0, label_center = 0;
        constexpr double r = 22;
        const double cx = a.center();
        double cy = 0, translate_y = 0;
        if (classic_glyph(a)) {
            // the label sits at the y drawText was given plus half the height of the actor's rect, which is the height the actor has now
            const double actor_y = footer ? a.stopy : a.starty;
            const double rect_height = footer ? classic_glyph_height(a) : a.height;
            if (control) cy = actor_y + 32, label_center = actor_y + r + (footer ? 5 : 12) + rect_height / 2;
            else if (boundary) cy = actor_y + 12, label_center = actor_y + 15 + rect_height / 2, translate_y = r / 2 + 10;
            else cy = actor_y + (footer ? 10 : 25), label_center = actor_y + (footer ? 15 : 30) + rect_height / 2, translate_y = footer ? r : r / 2 - 5;
            glyph_bottom = cy + r;
        } else {
            neo_bands(a, footer, glyph_bottom, label_center);
            cy = glyph_bottom - 22;
        }
        SvgWriter::Attrs attrs{{"class", std::string(control || boundary ? "actor-man" : "actor") + (footer ? " actor-bottom" : " actor-top")}, {"name", a.actor->key}};
        if (!control && conf_.neo) attrs.emplace_back("filter", "url(#" + id + "-drop-shadow)");
        std::string style = actor_style(a);
        if (boundary && style.empty()) {  // a boundary is styled with the theme's border only
            const std::string& border = theme_.sequence.at("actorBorder");
            const auto c = parse_css_color(border);
            style = "stroke: " + (c ? css_rgb(*c) : border) + ";";
        }
        if (control && style.empty()) {  // a control is always styled: the theme's border and background
            const auto rgb = [](const std::string& css) {
                const auto c = parse_css_color(css);
                return c ? css_rgb(*c) : css;
            };
            style = "stroke: " + rgb(theme_.sequence.at("actorBorder")) + "; fill: " + rgb(theme_.sequence.at("actorBkg")) + ";";
        }
        const bool classic = classic_glyph(a);
        if (classic && translate_y != 0)  // the classic look shifts the finished glyph and its label
            attrs.emplace_back("transform", boundary ? "translate(0," + num(translate_y) + ")" : "translate(0, " + num(translate_y) + ")");
        if (!style.empty() && !classic) attrs.emplace_back("style", style);
        if (!footer) {
            attrs.emplace_back("data-et", "participant");
            attrs.emplace_back("data-type", a.actor->type);
            attrs.emplace_back("data-id", a.actor->key);
        }
        if (!style.empty() && classic) attrs.emplace_back("style", style);  // there the style is set after the data attributes
        out.open("g", attrs);
        if (boundary) {
            const std::string count = std::to_string(footer ? visible_count_ - 1 : a.draw_index);
            const double icon_y = glyph_bottom - r;
            out.leaf("line", {{"id", "actor-man-torso" + count}, {"x1", num(cx - r * 2.5)}, {"y1", num(icon_y)}, {"x2", num(cx - 15)}, {"y2", num(icon_y)}});
            out.leaf("line", {{"id", "actor-man-arms" + count}, {"x1", num(cx - r * 2.5)}, {"y1", num(icon_y - 10)}, {"x2", num(cx - r * 2.5)}, {"y2", num(icon_y + 10)}});
            out.leaf("circle", {{"cx", num(cx)}, {"cy", num(icon_y)}, {"r", num(r)}});
        } else if (control) {
            out.raw("<defs><marker id=\"" + id + "-filled-head-control\" refX=\"11\" refY=\"5.8\" markerWidth=\"20\" markerHeight=\"28\" orient=\"172.5\" stroke-width=\"1.2\">"
                    "<path d=\"M 14.4 5.6 L 7.2 10.4 L 8.8 5.6 L 7.2 0.8 Z\"></path></marker></defs>");
            out.leaf("circle", {{"cx", num(cx)}, {"cy", num(cy)}, {"r", num(r)}, {"filter", conf_.neo ? "url(#" + id + "-drop-shadow)" : std::string()}});
            out.leaf("line", {{"marker-end", "url(#" + id + "-filled-head-control)"}, {"transform", "translate(" + num(cx) + ", " + num(cy - r) + ")"}});
        } else {
            out.leaf("circle", {{"cx", num(cx)}, {"cy", num(cy)}, {"r", num(r)}, {"width", num(a.width)}, {"height", num(classic && footer ? classic_glyph_height(a) : a.height)}});
            out.leaf("line", {{"x1", num(cx - r)}, {"x2", num(cx + r)}, {"y1", num(cy + r)}, {"y2", num(cy + r)}, {"stroke-width", "2"}});
        }
        write_label_rows(out, a.label, cx, label_center, "actor actor-man");
        out.close();
    }

    void write_man(SvgWriter& out, const actor_box& a, bool footer) const {
        constexpr double kTypeWidth = 18 * 2, kGlyphTop = -5, kGlyphBottom = 60, kGlyphHeight = kGlyphBottom - kGlyphTop;
        const double actor_y = footer ? a.stopy : a.starty;
        const double center = a.center();
        const double text_h = measure(a.label).height;
        double glyph_bottom = 0, label_center = 0;
        if (!conf_.neo) {  // the classic figure: 65px of glyph from the top of its box, the label 35px below it
            glyph_bottom = actor_y + kGlyphBottom;
            label_center = actor_y + 35 + 65.0 / 2;
        } else if (footer) {
            glyph_bottom = actor_y + kLifelineGap + kGlyphBand;
            label_center = glyph_bottom + kGlyphGap + text_h / 2;
        } else {
            const double lifeline_start = actor_y + a.height;
            glyph_bottom = lifeline_start - kLifelineGap - text_h - kGlyphGap;
            label_center = lifeline_start - kLifelineGap - text_h / 2;
        }
        const double scale = conf_.neo ? kGlyphBand / kGlyphHeight : 1.0;
        const auto gy = [&](double offset) { return glyph_bottom - (kGlyphBottom - offset) * scale; };
        const auto gx = [&](double offset) { return center + offset * scale; };
        // the ids carry the running actor count: the top figures their own, every bottom one the last
        const std::string count = std::to_string(footer ? visible_count_ - 1 : a.draw_index);
        out.open("g", [&] {
            SvgWriter::Attrs attrs{{"class", footer ? "actor-man actor-bottom" : "actor-man actor-top"}, {"name", a.actor->key}};
            if (!footer) {
                attrs.emplace_back("data-et", "participant");
                attrs.emplace_back("data-type", "actor");
                attrs.emplace_back("data-id", a.actor->key);
            }
            // a figure is always styled: the colour arrays of the redux themes, else the border of the theme (as rgb())
            const std::string style = actor_style(a);
            if (!style.empty()) {
                attrs.emplace_back("style", style);
            } else {
                const std::string& border = theme_.sequence.at("actorBorder");
                const auto c = parse_css_color(border);
                attrs.emplace_back("style", "stroke: " + (c ? css_rgb(*c) : border) + ";");
            }
            return attrs;
        }());
        out.leaf("line", {{"id", "actor-man-torso" + count}, {"x1", num(center)}, {"y1", num(gy(25))}, {"x2", num(center)}, {"y2", num(gy(45))}});
        out.leaf("line", {{"id", "actor-man-arms" + count}, {"x1", num(gx(-kTypeWidth / 2))}, {"y1", num(gy(33))},
                          {"x2", num(gx(kTypeWidth / 2))}, {"y2", num(gy(33))}});
        out.leaf("line", {{"x1", num(gx(-kTypeWidth / 2))}, {"y1", num(gy(60))}, {"x2", num(center)}, {"y2", num(gy(45))}});
        out.leaf("line", {{"x1", num(center)}, {"y1", num(gy(45))}, {"x2", num(gx(kTypeWidth / 2 - 2))}, {"y2", num(gy(60))}});
        out.leaf("circle", {{"cx", num(center)}, {"cy", num(gy(10))}, {"r", num(15 * scale)}, {"width", num(a.width)}, {"height", num(a.height)}});
        write_label_rows(out, a.label, center, label_center, "actor actor-man");
        out.close();
    }

    // The lowered part of an actor: for a figure only the lifeline, for a participant the box too.
    void write_group_box(SvgWriter& out, const group_box& g) const {
        out.open("g");
        out.leaf("rect", {{"x", num(g.startx)}, {"y", num(g.starty)}, {"fill", g.box->fill.value_or("transparent")},
                          {"stroke", "rgb(0,0,0, 0.5)"}, {"width", num(g.stopx - g.startx)}, {"height", num(g.stopy - g.starty)},
                          {"class", "rect"}});
        if (!g.box->name.empty()) {
            const double x = g.x + g.width / 2;
            write_label_rows(out, g.text, x, g.y + conf_.box_text_margin + box_text_height_ / 2, "text");
        }
        out.close();
    }

    // fixLifeLineHeights: with mirrored actors a lifeline ends at the top of the mirror; without them only a destroyed
    // actor's lifeline is cut (at the middle of its box), the others keep the 2000 they were drawn with
    double lifeline_end(const actor_box& a) const {
        if (conf_.mirror_actors) return a.stopy;
        // fixLifeLineHeights runs after the drawing, so a classic glyph has the height its getBBox() gave it
        return a.stopy != 0 ? a.stopy + (classic_glyph(a) ? classic_glyph_height(a) : a.height) / 2 : 2000;
    }

    void write_actor(SvgWriter& out, const actor_box& a, bool footer) const {
        const double y = footer ? a.stopy : a.starty;
        const std::string id = options_.svg_id;
        const bool man = a.actor->type == "actor" || a.actor->type == "control" || a.actor->type == "entity" || a.actor->type == "boundary";  // drawn in place, not in the lowered group
        const auto rect = [&] {
            SvgWriter::Attrs attrs{{"x", num(a.x)}, {"y", num(y)}, {"fill", rect_class(a).empty() ? "#eaeaea" : "#EDF2AE"}, {"stroke", "#666"}, {"width", num(a.width)},
                                   {"height", num(a.height)}, {"name", a.actor->key}, {"rx", conf_.neo ? "6" : "3"},
                                   {"ry", conf_.neo ? "6" : "3"}, {"class", rect_class_full(a, footer)}};
            if (conf_.neo) {
                attrs.emplace_back("data-look", "neo");
                attrs.emplace_back("filter", "url(#" + id + "-drop-shadow)");
            }
            if (const std::string style = actor_style(a); !style.empty()) attrs.emplace_back("style", style);
            out.leaf("rect", attrs);
            write_label_rows(out, a.label, a.center(), y + a.height / 2, "actor actor-box");
        };
        // `collections`: the box and a copy of it 6px to the lower left, the drop shadow on the group instead of the box
        const bool stacked = a.actor->type == "collections";
        const auto stacked_boxes = [&] {
            constexpr double offset = 6;
            const double height = conf_.neo ? a.height - offset : a.height;
            const auto box = [&](double dx, double dy, const std::string& cls) {
                SvgWriter::Attrs attrs{{"x", num(a.x + dx)}, {"y", num(y + dy)}, {"fill", rect_class(a).empty() ? "#eaeaea" : "#EDF2AE"}, {"stroke", "#666"},
                                       {"width", num(a.width)}, {"height", num(height)}, {"name", a.actor->key}, {"class", cls}};
                if (conf_.neo) attrs.emplace_back("data-look", "neo");
                if (const std::string style = actor_style(a); !style.empty()) attrs.emplace_back("style", style);
                out.leaf("rect", attrs);
            };
            box(0, 0, rect_class_full(a, footer));
            box(-offset, offset, "actor");
            write_label_rows(out, a.label, a.center() - offset, y + offset + height / 2, "actor actor-box");
        };
        // `queue`: a sideways cylinder, the body translated to the left by its arc and the front arc a group of its own
        const bool queue = a.actor->type == "queue";
        const auto queue_shape = [&] {
            const double ry = a.height / 2;
            const double rx = ry / (2.5 + a.height / 50);
            const std::string style = actor_style(a);
            const auto with_style = [&](SvgWriter::Attrs attrs) {
                if (!style.empty()) attrs.emplace_back("style", style);
                return attrs;
            };
            SvgWriter::Attrs body{{"transform", "translate(" + num(rx) + ", " + num(-(a.height / 2)) + ")"}};
            if (conf_.neo) body.emplace_back("filter", "url(#" + id + "-drop-shadow)");
            out.open("g", with_style(body));
            out.leaf("path", {{"d", "M " + num(a.x) + "," + num(y + ry) + "\n    a " + num(rx) + "," + num(ry) + " 0 0 0 0," + num(a.height) +
                                        "\n    h " + num(a.width - 2 * rx) + "\n    a " + num(rx) + "," + num(ry) + " 0 0 0 0,-" + num(a.height) +
                                        "\n    Z"}});
            out.close();
            out.open("g", with_style({{"transform", "translate(" + num(a.width - rx) + ", " + num(-a.height / 2) + ")"}}));
            out.leaf("path", {{"d", "M " + num(a.x) + "," + num(y + ry) + "\n      a " + num(rx) + "," + num(ry) + " 0 0 0 0," + num(a.height)}});
            out.close();
            write_label_rows(out, a.label, a.center(), y + a.height / 2, "actor actor-box");
        };
        const bool database = a.actor->type == "database";
        const auto database_shape = [&] {
            double glyph_bottom;
            double label_center;
            neo_bands(a, footer, glyph_bottom, label_center);
            const double w = a.width / 3, rx = w / 2, ry = rx / (2.5 + w / 50), h = kGlyphBand;
            const double cylinder_y = glyph_bottom - h - ry;
            const std::string cls = rect_class_full(a, footer);
            const std::string style = actor_style(a);
            SvgWriter::Attrs cylinder{{"class", cls}};
            if (conf_.neo) cylinder.emplace_back("filter", "url(#" + id + "-drop-shadow)");
            cylinder.emplace_back("transform", "translate(" + num(w) + ", " + num(ry) + ")");
            if (!style.empty()) cylinder.emplace_back("style", style);
            else {
                const std::string& border = theme_.sequence.at("actorBorder");
                const auto c = parse_css_color(border);
                cylinder.emplace_back("style", "stroke: " + (c ? css_rgb(*c) : border) + ";");
            }
            out.open("g", cylinder);
            out.leaf("path", {{"d", "M " + num(a.x) + "," + num(cylinder_y + ry) + "\n  a " + num(rx) + "," + num(ry) + " 0 0 0 " + num(w) + ",0\n  a " + num(rx) +
                                        "," + num(ry) + " 0 0 0 -" + num(w) + ",0\n  l 0," + num(h - 2 * ry) + "\n  a " + num(rx) + "," + num(ry) + " 0 0 0 " + num(w) +
                                        ",0\n  l 0,-" + num(h - 2 * ry)}});
            out.close();
            write_label_rows(out, a.label, a.center(), label_center, "actor actor-box");
        };
        if (footer) {
            if (database) {
                out.open("g");
                database_shape();
                out.close();
                return;
            }
            if (queue) {
                out.open("g", {{"class", rect_class_full(a, true)}});
                queue_shape();
                out.close();
                return;
            }
            out.open("g", stacked && conf_.neo ? SvgWriter::Attrs{{"filter", "url(#" + id + "-drop-shadow)"}} : SvgWriter::Attrs{});
            if (stacked) stacked_boxes();
            else if (!man) rect();
            out.close();
            return;
        }
        out.open("g");
        out.leaf("line", {{"id", "actor" + std::to_string(a.draw_index)}, {"x1", num(a.center())}, {"y1", num(classic_glyph(a) ? y + (a.actor->type == "boundary" ? 80 : 75) : man && !conf_.neo ? y + 80 : y + a.height)},
                          {"x2", num(a.center())}, {"y2", num(lifeline_end(a))}, {"class", "actor-line 200"}, {"stroke-width", "0.5px"},
                          {"stroke", "#999"}, {"name", a.actor->key}, {"data-et", "life-line"}, {"data-id", a.actor->key}});
        if (!man) {
            SvgWriter::Attrs root{{"id", "root-" + std::to_string(a.draw_index)}};
            if (conf_.neo) root.emplace_back("data-look", "neo");
            if (queue) root.emplace_back("class", rect_class_full(a, false));
            if (stacked && conf_.neo) root.emplace_back("filter", "url(#" + id + "-drop-shadow)");
            root.emplace_back("data-et", "participant");
            root.emplace_back("data-type", a.actor->type);
            root.emplace_back("data-id", a.actor->key);
            out.open("g", root);
            if (stacked) stacked_boxes();
            else if (queue) queue_shape();
            else if (database) database_shape();
            else rect();
            out.close();
        }
        out.close();
    }

    std::string draw() {
        const std::string id = options_.svg_id;
        SvgWriter out;
        // the frame: viewBox = bounds + margins, the neo look adds room for the glyph bands
        double stopx = data_stopx_, startx = data_startx_, stopy = data_stopy_, starty = data_starty_;
        const popup_size popup = popup_extent();
        double box_height = MERMAID_MAX(stopy - starty, popup.height);
        double height = box_height + 2 * conf_.diagram_margin_y;
        if (conf_.mirror_actors) height += conf_.bottom_margin_adj - conf_.box_margin;
        const double width = MERMAID_MAX(stopx - startx, popup.width) + 2 * conf_.diagram_margin_x;
        const std::string w = num(width);
        const double title_room = model_.title.empty() ? 0 : 40;  // extraVertForTitle
        // configureSvgSize: a fluid svg (width 100%, max-width) or one of fixed size
        const std::string view_box = num(startx - conf_.diagram_margin_x) + " -" + num(conf_.diagram_margin_y + title_room) + " " + w + " " +
                                     num(height + title_room + (actors_.empty() || !conf_.neo ? 0 : kExtraHeightForNeoActors));
        SvgWriter::Attrs frame;
        if (conf_.use_max_width)
            frame = {{"id", id}, {"width", "100%"}, {"xmlns", "http://www.w3.org/2000/svg"}, {"xmlns:xlink", "http://www.w3.org/1999/xlink"},
                     {"style", "max-width: " + w + "px; background-color: " + options_.background + ";"}, {"viewBox", view_box}};
        else
            frame = {{"id", id}, {"width", w}, {"xmlns", "http://www.w3.org/2000/svg"}, {"xmlns:xlink", "http://www.w3.org/1999/xlink"},
                     {"height", num(height)}, {"viewBox", view_box}};
        frame.emplace_back("role", "graphics-document document");
        frame.emplace_back("aria-roledescription", "sequence");
        if (!model_.acc_descr.empty()) frame.emplace_back("aria-describedby", "chart-desc-" + id);
        if (!model_.acc_title.empty()) frame.emplace_back("aria-labelledby", "chart-title-" + id);
        out.open("svg", frame);
        if (!model_.acc_title.empty()) out.open("title", {{"id", "chart-title-" + id}}).text(model_.acc_title).close();
        if (!model_.acc_descr.empty()) out.open("desc", {{"id", "chart-desc-" + id}}).text(model_.acc_descr).close();
        // each box is lowered when it is drawn, after the `rect` backgrounds: the last one comes first
        for (auto it = boxes_.rbegin(); it != boxes_.rend(); ++it)
            if (it->used) write_group_box(out, *it);
        // the backgrounds of `rect` are lowered last of all: the last one comes first
        for (auto it = backgrounds_.rbegin(); it != backgrounds_.rend(); ++it)
            out.leaf("rect", {{"x", num(it->startx.value_or(0))}, {"y", num(it->starty.value_or(0))}, {"fill", it->fill},
                              {"width", num(it->stopx.value_or(0) - it->startx.value_or(0))},
                              {"height", num(it->stopy.value_or(0) - it->starty.value_or(0))}, {"class", "rect"}});
        // groups are inserted at the front (`.lower()`): the last drawn comes first
        if (conf_.mirror_actors)
            for (auto it = actors_.rbegin(); it != actors_.rend(); ++it)
                if (it->visible) write_actor(out, *it, true);
        for (auto it = actors_.rbegin(); it != actors_.rend(); ++it)
            if (it->visible) write_actor(out, *it, false);
        out.open("style");
        out.raw(detail::sequence_stylesheet(theme_, id, conf_.neo));
        out.close();
        out.open("g");
        out.close();
        for (const detail::sequence_icon& icon : detail::kSequenceIcons) {
            out.open("defs");
            out.raw("<symbol id=\"" + id + "-" + icon.name + "\"" + icon.attributes + "><path transform=\"scale(.5)\" d=\"" + icon.path + "\"></path></symbol>");
            out.close();
        }
        std::string defs = detail::kSequenceDefs;
        // insertDropShadow: black for the redux themes (the default one), white for the others
        if (!conf_.neo) defs.erase(defs.find("<defs><filter"));  // the drop shadow is the neo look's
        else if (theme_.border_color_array.empty()) defs.replace(defs.find("flood-color=\"#000000\""), 21, "flood-color=\"#FFFFFF\"");
        for (size_t at = defs.find("@ID@"); at != std::string::npos; at = defs.find("@ID@", at + id.size())) defs.replace(at, 4, id);
        out.raw(defs);
        for (const std::string& fragment : pre_) out.raw(fragment);  // notes and activation bars, in drawing order
        for (const actor_box& a : actors_)  // the figures are appended where they are drawn, not lowered
            if (a.visible && a.actor->type == "actor") write_man(out, a, false);
            else if (a.visible && (a.actor->type == "control" || a.actor->type == "entity" || a.actor->type == "boundary")) write_glyph(out, a, false);
        for (const message_box& m : messages_) write_message(out, m);
        for (const actor_box& a : actors_)
            if (a.visible && conf_.mirror_actors) {
                if (a.actor->type == "actor") write_man(out, a, true);
                else if (a.actor->type == "control" || a.actor->type == "entity" || a.actor->type == "boundary") write_glyph(out, a, true);
            }
        write_popups(out);
        if (!model_.title.empty()) {  // appended last, centred on the bounds
            out.open("text", {{"x", num((stopx - startx) / 2 - 2 * conf_.diagram_margin_x)}, {"y", "-25"}}).text(purify_ampersands(detail::decode_entity_codes(model_.title))).close();
        }
        out.close();
        return out.str();
    }

    // drawCentralConnection: a circle where the line meets the lifeline, at the end, the start or both
    void write_central_connection(SvgWriter& out, const Message& msg, const message_box& m) const {
        const actor_box& from = *find_actor(msg.from);
        const actor_box& to = *find_actor(msg.to);
        double from_center = from.center(), to_center = to.center();
        const bool left_to_right = from_center <= to_center;
        const bool reverse = is_reverse_arrow(msg.type);
        const auto offset = [&](bool ltr, bool rev) {
            const double base = ltr ? 16.5 : -16.5;
            return rev ? -base : base;
        };
        const int central = msg.central;
        if (m.autonumbered) {
            if (central == static_cast<int>(LineType::CentralConnection)) {
                if (reverse) to_center += offset(left_to_right, true);
            } else if (central == static_cast<int>(LineType::CentralConnectionReverse)) {
                if (!reverse) from_center += offset(left_to_right, false);
            } else if (reverse) {
                to_center += offset(left_to_right, true);
            } else {
                from_center += offset(left_to_right, false);
            }
        }
        const auto circle = [&](double cx) {
            out.leaf("circle", {{"cx", num(cx)}, {"cy", num(m.line_y)}, {"r", "5"}, {"width", "10"}, {"height", "10"}});
        };
        out.open("g");
        if (central == static_cast<int>(LineType::CentralConnection)) circle(to_center);
        else if (central == static_cast<int>(LineType::CentralConnectionReverse)) circle(from_center);
        else {
            circle(from_center);
            circle(to_center);
        }
        out.close();
    }

    void write_message(SvgWriter& out, const message_box& m) const {
        const Message& msg = *m.message;
        std::string anchor;
        const double x = aligned_x(conf_.message_align, MERMAID_MIN(m.startx, m.stopx), std::abs(m.stopx - m.startx), conf_.wrap_padding, anchor);
        write_rows(out, detail::split_breaks(m.text), x, m.starty + 10, "messageText", conf_.wrap_padding, false, anchor);
        const bool dotted = is_dotted(msg.type);
        const bool self = m.startx == m.stopx;
        SvgWriter::Attrs attrs;
        const bool bidirectional = msg.type == LineType::BidirectionalSolid || msg.type == LineType::BidirectionalDotted;
        if (self) {
            // with a number circle a bidirectional loop starts 10px to the right (Mermaid's lineStartX)
            const double start = m.startx + (m.numbered && (bidirectional || is_reverse_arrow(msg.type)) ? 10 : 0);
            if (conf_.right_angles)
                attrs = {{"d", "M  " + num(start) + "," + num(m.line_y) + " H " + num(m.startx + MERMAID_MAX(conf_.width / 2, measure(m.text).width / 2)) +
                                   " V " + num(m.line_y + 25) + " H " + num(m.startx)}};
            else
            attrs = {{"d", "M " + num(start) + "," + num(m.line_y) + " C " + num(start + 60) + "," + num(m.line_y - 10) + " " +
                               num(m.startx + 60) + "," + num(m.line_y + 30) + " " + num(m.startx) + "," + num(m.line_y + 20)}};
        } else {
            attrs = {{"x1", num(m.startx)}, {"y1", num(m.line_y)}, {"x2", num(m.stopx)}, {"y2", num(m.line_y)}};
        }
        attrs.insert(attrs.end(), {{"class", dotted ? "messageLine1" : "messageLine0"}, {"data-et", "message"},
                               {"data-id", "i" + msg.id}, {"data-from", msg.from}, {"data-to", msg.to},
                               {"stroke-width", "2"}, {"stroke", "none"}});
        const std::string base = "url(#" + options_.svg_id + "-";
        switch (msg.type) {
            case LineType::Solid: case LineType::Dotted: attrs.emplace_back("marker-end", base + "arrowhead)"); break;
            case LineType::BidirectionalSolid: case LineType::BidirectionalDotted:
                attrs.emplace_back("marker-start", base + "arrowhead)");
                attrs.emplace_back("marker-end", base + "arrowhead)");
                break;
            case LineType::SolidPoint: case LineType::DottedPoint: attrs.emplace_back("marker-end", base + "filled-head)"); break;
            case LineType::SolidCross: case LineType::DottedCross: attrs.emplace_back("marker-end", base + "crosshead)"); break;
            case LineType::SolidTop: case LineType::SolidTopDotted: attrs.emplace_back("marker-end", base + "solidTopArrowHead)"); break;
            case LineType::SolidBottom: case LineType::SolidBottomDotted: attrs.emplace_back("marker-end", base + "solidBottomArrowHead)"); break;
            case LineType::StickTop: case LineType::StickTopDotted: attrs.emplace_back("marker-end", base + "stickTopArrowHead)"); break;
            case LineType::StickBottom: case LineType::StickBottomDotted: attrs.emplace_back("marker-end", base + "stickBottomArrowHead)"); break;
            // a reverse arrow draws the head of its mirror image at the start
            case LineType::SolidArrowTopReverse: case LineType::SolidArrowTopReverseDotted: attrs.emplace_back("marker-start", base + "solidBottomArrowHead)"); break;
            case LineType::SolidArrowBottomReverse: case LineType::SolidArrowBottomReverseDotted: attrs.emplace_back("marker-start", base + "solidTopArrowHead)"); break;
            case LineType::StickArrowTopReverse: case LineType::StickArrowTopReverseDotted: attrs.emplace_back("marker-start", base + "stickBottomArrowHead)"); break;
            case LineType::StickArrowBottomReverse: case LineType::StickArrowBottomReverseDotted: attrs.emplace_back("marker-start", base + "stickTopArrowHead)"); break;
            default: break;
        }
        attrs.emplace_back("style", std::string(dotted ? "stroke-dasharray: 3, 3; " : "") + "fill: none;");
        if (m.numbered) {
            // the line starts after the circle (a bidirectional one is moved by the circle's diameter); on a self message
            // the path has no x1, but Mermaid sets the attribute all the same
            const double radius = 6;
            double x1 = m.startx + radius;
            const bool reverse = is_reverse_arrow(msg.type);
            const bool central = msg.central != 0;
            const double back = msg.central == static_cast<int>(LineType::CentralConnectionDual) || msg.central == static_cast<int>(LineType::CentralConnectionReverse) ? -7.5 : 0;
            if (bidirectional) x1 = m.startx < m.stopx ? m.startx + 2 * radius : m.startx - radius + (central ? -5 : 0) + back;
            if (reverse) x1 = m.startx + (m.stopx > m.startx ? 0 : back);
            if (self) attrs.emplace_back("x1", num(x1));
            else for (auto& a : attrs) if (a.first == "x1") a.second = num(x1);
            if (reverse) {  // the line ends before the circle at the other end
                const double x2 = (m.stopx > m.startx ? m.stopx - 2 * radius : m.stopx - radius) + (central ? 15 : 0);
                if (self) attrs.emplace_back("x2", num(x2));
                else for (auto& a : attrs) if (a.first == "x2") a.second = num(x2);
            }
        }
        out.leaf(self ? "path" : "line", attrs);
        if (msg.central) write_central_connection(out, msg, m);
        if (m.numbered) {
            const double x = self ? m.from_bounds + 1
                             : is_reverse_arrow(msg.type) ? (m.startx <= m.stopx ? m.to_bounds - 1 : m.from_bounds + 1)
                                                          : (m.startx <= m.stopx ? m.from_bounds + 1 : m.to_bounds - 1);
            const size_t length = num(m.number).size();
            const char* font_size = length > 5 ? "7px" : length > 3 ? "9px" : "12px";
            out.leaf("line", {{"x1", num(x)}, {"y1", num(m.line_y)}, {"x2", num(x)}, {"y2", num(m.line_y)}, {"stroke-width", "0"},
                              {"marker-start", base + "sequencenumber)"}});
            out.open("text", {{"x", num(x)}, {"y", num(m.line_y + 4)}, {"font-family", "sans-serif"}, {"font-size", font_size},
                              {"text-anchor", "middle"}, {"class", "sequenceNumber"}});
            out.text(num(m.number));
            out.close();
        }
    }

    const Model& model_;
    conf conf_;
    RenderOptions options_;
    detail::theme_variables theme_;
    std::vector<actor_box> actors_;
    std::vector<message_box> messages_;
    size_t visible_count_ = 0;
    std::vector<group_box> boxes_;  // the `box`es, in the order of the model
    double box_text_height_ = 0;     // textMaxHeight: the tallest box title
    std::vector<activation> open_;  // bounds.activations while the statements are walked
    std::vector<loop_item> items_;  // bounds.sequenceItems
    std::vector<loop_item> backgrounds_;  // the `rect`s, drawn behind everything at the end
    std::vector<double> loop_width_;       // calculateLoopBounds: width of a fragment, shared by its sections
    std::map<std::string, size_t> loop_slot_;  // statement id -> slot in loop_width_
    std::vector<std::string> pre_;  // what is appended while the statements are walked: notes, activation anchors
    double actor_height_ = 0, vertical_ = 0;
    bool has_bounds_ = false;
    double number_ = 1, number_step_ = 1;  // `autonumber`: the next index and its step
    bool numbers_on_ = false;
    double data_startx_ = 0, data_stopx_ = 0, data_starty_ = 0, data_stopy_ = 0;
};

}  // namespace

std::string render_svg(const Model& model, const RenderOptions& options) { return Drawing(model, options).render(); }

}  // namespace mermaid::sequence
