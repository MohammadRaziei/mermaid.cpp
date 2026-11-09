// The parser against what Mermaid 12.2.0's sequenceDb holds for the same text (tests/data/sequence/*.json, written by
// tools/sequence_db.mjs from real Mermaid).
#include <fstream>
#include <sstream>
#include <string>

#include <mermaid/error.h>
#include <mermaid/sequence_parser.h>

#include "../../src/json.h"
#include "utest.h"

using mermaid::detail::json_reader;
using mermaid::detail::json_value;
using namespace mermaid::sequence;

namespace {

const std::string kData = std::string(MERMAID_TEST_DATA_DIR) + "/sequence/";

std::string read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string str(const json_value& obj, const char* key, const std::string& fallback = "") {
    const json_value* v = obj.find(key);
    return v && v->type == json_value::kind::string ? v->string : fallback;
}

// One fixture: every message and actor the golden lists is in the model, with the same fields.
std::string compare(const std::string& name) {
    const std::string source = read(kData + name + ".mmd");
    const std::string golden_text = read(kData + name + ".json");
    const auto golden = json_reader(golden_text).read_object();
    if (!golden) return name + ": golden unreadable";
    Model model;
    try {
        model = Parser().parse(source);
    } catch (const std::exception& e) {
        return name + ": " + e.what();
    }
    std::ostringstream problems;
    if (model.title != str(*golden, "title")) problems << " title '" << model.title << "'";
    const json_value& actors = *golden->find("actors");
    if (actors.items.size() != model.actors.size()) problems << " actors " << model.actors.size() << " vs " << actors.items.size();
    for (size_t i = 0; i < actors.items.size() && i < model.actors.size(); ++i) {
        const json_value& g = actors.items[i];
        const Actor& a = model.actors[i];
        if (a.key != str(g, "key") || a.description != str(g, "description") || a.type != str(g, "type"))
            problems << " actor " << i << " " << a.key << "/" << a.description << "/" << a.type;
        for (const char* field : {"links", "properties"}) {
            const json_value* golden_pairs = g.find(field);
            const auto& ours = std::string(field) == "links" ? a.links : a.properties;
            if (!golden_pairs || golden_pairs->members.size() != ours.size()) {
                problems << " actor " << a.key << " " << field;
                continue;
            }
            for (size_t k = 0; k < ours.size(); ++k)
                if (golden_pairs->order[k] != ours[k].first || str(*golden_pairs, ours[k].first.c_str()) != ours[k].second)
                    problems << " actor " << a.key << " " << field << " " << ours[k].first;
        }
        const json_value* box = g.find("box");
        if ((box != nullptr) != (a.box >= 0)) problems << " actor " << a.key << " box";
        else if (box && model.boxes[static_cast<size_t>(a.box)].name != str(*box, "name")) problems << " actor " << a.key << " box name";
    }
    const json_value& boxes = *golden->find("boxes");
    if (boxes.items.size() != model.boxes.size()) problems << " boxes";
    for (size_t i = 0; i < boxes.items.size() && i < model.boxes.size(); ++i) {
        const json_value* fill = boxes.items[i].find("fill");
        if (model.boxes[i].name != str(boxes.items[i], "name") || (fill != nullptr) != model.boxes[i].fill.has_value() ||
            (fill && fill->string != *model.boxes[i].fill))
            problems << " box " << i;
    }
    const json_value& messages = *golden->find("messages");
    if (messages.items.size() != model.messages.size()) problems << " messages " << model.messages.size() << " vs " << messages.items.size();
    for (size_t i = 0; i < messages.items.size() && i < model.messages.size(); ++i) {
        const json_value& g = messages.items[i];
        const Message& m = model.messages[i];
        const json_value* text = g.find("message");
        const bool structured = text && text->type == json_value::kind::object;  // autonumber carries an object
        if (m.id != str(g, "id") || static_cast<int>(m.type) != static_cast<int>(g.find("type")->number))
            problems << " message " << i << " id/type";
        if (m.from != str(g, "from") || m.to != str(g, "to")) problems << " message " << i << " actors " << m.from << ">" << m.to;
        if (!structured && m.text != str(g, "message")) problems << " message " << i << " text '" << m.text << "'";
        if (const json_value* p = g.find("placement"); p && (!m.placement || static_cast<int>(*m.placement) != static_cast<int>(p->number)))
            problems << " message " << i << " placement";
        if (const json_value* a = g.find("activate"); a && (a->number != 0) != m.activate) problems << " message " << i << " activate";
        if (structured) {
            const json_value* visible = text->find("visible");
            if (!m.number_visible || *m.number_visible != (visible && visible->number != 0)) problems << " message " << i << " visible";
            if (const json_value* start = text->find("start"); start && (!m.number_start || *m.number_start != static_cast<int>(start->number)))
                problems << " message " << i << " start";
            if (const json_value* step = text->find("step"); step && (!m.number_step || *m.number_step != static_cast<int>(step->number)))
                problems << " message " << i << " step";
        }
    }
    const json_value& created = *golden->find("createdActors");
    if (created.items.size() != model.created_actors.size()) problems << " created";
    for (const json_value& pair : created.items) {
        const auto it = model.created_actors.find(pair.items[0].string);
        if (it == model.created_actors.end() || it->second != static_cast<size_t>(pair.items[1].number)) problems << " created " << pair.items[0].string;
    }
    const json_value& destroyed = *golden->find("destroyedActors");
    if (destroyed.items.size() != model.destroyed_actors.size()) problems << " destroyed";
    for (const json_value& pair : destroyed.items) {
        const auto it = model.destroyed_actors.find(pair.items[0].string);
        if (it == model.destroyed_actors.end() || it->second != static_cast<size_t>(pair.items[1].number)) problems << " destroyed " << pair.items[0].string;
    }
    const std::string out = problems.str();
    return out.empty() ? "" : name + ":" + out;
}

}  // namespace

#define SEQUENCE_FIXTURE(name)                       \
    UTEST(sequence_parser, matches_mermaid_##name) { \
        const std::string problems = compare(#name); \
        ASSERT_STREQ("", problems.c_str());          \
    }

SEQUENCE_FIXTURE(seq_basic)
SEQUENCE_FIXTURE(seq_rich)
SEQUENCE_FIXTURE(seq_arrows)
SEQUENCE_FIXTURE(seq_half_arrows)
SEQUENCE_FIXTURE(seq_half_arrows_dotted)
SEQUENCE_FIXTURE(seq_central)
SEQUENCE_FIXTURE(seq_central_autonumber)
SEQUENCE_FIXTURE(seq_notes)
SEQUENCE_FIXTURE(seq_fragments)
SEQUENCE_FIXTURE(seq_autonumber)
SEQUENCE_FIXTURE(seq_activation)
SEQUENCE_FIXTURE(seq_title_acc)
SEQUENCE_FIXTURE(seq_implicit_actors)
SEQUENCE_FIXTURE(seq_box)
SEQUENCE_FIXTURE(seq_box_color)
SEQUENCE_FIXTURE(seq_box_plain)
SEQUENCE_FIXTURE(seq_box_mixed)
SEQUENCE_FIXTURE(seq_create_destroy)
SEQUENCE_FIXTURE(seq_create_destroy_mixed)
SEQUENCE_FIXTURE(seq_wrap)
SEQUENCE_FIXTURE(seq_wrap_notes)
SEQUENCE_FIXTURE(seq_actor_labels)
SEQUENCE_FIXTURE(seq_rect_plain)
SEQUENCE_FIXTURE(seq_box_wrap)
SEQUENCE_FIXTURE(seq_init_spacing)
SEQUENCE_FIXTURE(seq_init_mirror)
SEQUENCE_FIXTURE(seq_init_mirror_destroy)
SEQUENCE_FIXTURE(seq_init_numbers)
SEQUENCE_FIXTURE(seq_init_wrap)
SEQUENCE_FIXTURE(seq_init_misc)
SEQUENCE_FIXTURE(seq_look_classic)
SEQUENCE_FIXTURE(seq_theme_forest_mixed)
SEQUENCE_FIXTURE(seq_links)
SEQUENCE_FIXTURE(seq_links_more)
SEQUENCE_FIXTURE(seq_links_classic)
SEQUENCE_FIXTURE(seq_links_wide)
SEQUENCE_FIXTURE(seq_par_over)
SEQUENCE_FIXTURE(seq_entities)
SEQUENCE_FIXTURE(seq_look_classic_mixed)
SEQUENCE_FIXTURE(seq_theme_variables)
SEQUENCE_FIXTURE(seq_theme_base)
SEQUENCE_FIXTURE(seq_theme_neutral)
SEQUENCE_FIXTURE(seq_theme_forest)
SEQUENCE_FIXTURE(seq_theme_dark)

UTEST(sequence_parser, accessibility_texts_are_read) {
    const Model m = Parser().parse("sequenceDiagram\n  accTitle: T\n  accDescr {\n    line one\n    line two\n  }\n  A->>B: x\n");
    ASSERT_STREQ("T", m.acc_title.c_str());
    ASSERT_STREQ("line one\nline two", m.acc_descr.c_str());
}

UTEST(sequence_parser, semicolons_separate_statements_and_hashes_start_comments) {
    const Model m = Parser().parse("sequenceDiagram\n  A->>B: one; B->>A: two # a comment\n");
    ASSERT_EQ(2u, m.messages.size());
    ASSERT_STREQ("two", m.messages[1].text.c_str());
}

UTEST(sequence_parser, a_missing_end_and_an_unknown_statement_are_errors) {
    bool threw = false;
    try { Parser().parse("sequenceDiagram\n  loop x\n  A->>B: y\n"); } catch (const mermaid::ParseError&) { threw = true; }
    ASSERT_TRUE(threw);
    threw = false;
    try { Parser().parse("sequenceDiagram\n  nonsense\n"); } catch (const mermaid::ParseError& e) { threw = e.line() == 2; }
    ASSERT_TRUE(threw);
}
