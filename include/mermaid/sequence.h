// sequence.h - the model of a `sequenceDiagram` (what Mermaid's sequenceDb keeps).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mermaid::sequence {

// sequenceDb LINETYPE: the numbers are Mermaid's, the golden dumps in tests/data/sequence use them.
enum class LineType : int {
    Solid = 0,
    Dotted = 1,
    Note = 2,
    SolidCross = 3,
    DottedCross = 4,
    SolidOpen = 5,
    DottedOpen = 6,
    LoopStart = 10,
    LoopEnd = 11,
    AltStart = 12,
    AltElse = 13,
    AltEnd = 14,
    OptStart = 15,
    OptEnd = 16,
    ActiveStart = 17,
    ActiveEnd = 18,
    ParStart = 19,
    ParAnd = 20,
    ParEnd = 21,
    RectStart = 22,
    RectEnd = 23,
    SolidPoint = 24,
    DottedPoint = 25,
    Autonumber = 26,
    CriticalStart = 27,
    CriticalOption = 28,
    CriticalEnd = 29,
    BreakStart = 30,
    BreakEnd = 31,
    ParOverStart = 32,
    BidirectionalSolid = 33,
    BidirectionalDotted = 34,
    // `A->>()B` starts an activation of B (59), `A()->>B` one of A (60); a message carries `central` (59, 60 or 61 for both ends)
    CentralConnection = 59,
    CentralConnectionReverse = 60,
    CentralConnectionDual = 61,
    // half arrows: the head on one side of the line only (`-|\`, `-|/`, `-\\`, `-//`), and the same at the start (`/|-`, `\|-`, `//-`, `\\-`); +10 when dotted
    SolidTop = 41,
    SolidBottom = 42,
    StickTop = 43,
    StickBottom = 44,
    SolidArrowTopReverse = 45,
    SolidArrowBottomReverse = 46,
    StickArrowTopReverse = 47,
    StickArrowBottomReverse = 48,
    SolidTopDotted = 51,
    SolidBottomDotted = 52,
    StickTopDotted = 53,
    StickBottomDotted = 54,
    SolidArrowTopReverseDotted = 55,
    SolidArrowBottomReverseDotted = 56,
    StickArrowTopReverseDotted = 57,
    StickArrowBottomReverseDotted = 58,
};

enum class Placement { LeftOf = 0, RightOf = 1, Over = 2 };

struct Actor {
    std::string key;          // the id used in messages
    std::string description;  // the label (`participant A as Alice`), the key by default
    std::string type = "participant";  // `participant` or `actor`
    bool wrap = false;
    int box = -1;             // index into Model::boxes, -1 outside a box
    // `link`/`links` (the menu of the actor: label, url) and `properties` (`class`, `icon`), in the order they were given
    std::vector<std::pair<std::string, std::string>> links, properties;
};

struct Box {
    std::string name;
    std::optional<std::string> fill;  // `box rgb(..) title`: the colour, if the line starts with one
    bool wrap = false;
    std::vector<std::string> actor_keys;
};

struct Message {
    std::string id;           // position in the list, as a string (Mermaid's)
    LineType type = LineType::Solid;
    std::string from, to;     // empty when the statement has none (fragments, autonumber)
    std::string text;         // message text, note text, fragment title, rect colour
    bool wrap = false;
    bool activate = false;    // `->>+`
    int central = 0;          // 0, or the CentralConnection* value of the message's `()` marks
    std::optional<Placement> placement;  // notes
    // `autonumber [start [step]]` / `autonumber off`
    std::optional<int> number_start, number_step;
    std::optional<bool> number_visible;
};

// The `sequence` part of the `%%{init: ...}%%` directives (and the top-level `wrap`) that the renderer reads; later directives win.
struct Config {
    std::optional<double> width, height, actor_margin, box_margin, box_text_margin, note_margin, message_margin, wrap_padding;
    std::optional<double> diagram_margin_x, diagram_margin_y, bottom_margin_adj, activation_width, label_box_width, label_box_height;
    // the top-level `fontSize` in px (a number or `"14px"`): Mermaid sets the actor, note and message font sizes to it, so the sequence
    // keys of those names change nothing
    std::optional<double> font_size;
    std::optional<bool> mirror_actors, wrap, show_sequence_numbers, use_max_width, right_angles, hide_unused_participants;
    std::optional<std::string> message_align, note_align;  // left / center / right (also start / middle / end)
    // the theme side of the directives, as flowchart::InitConfig reads it: theme, fontFamily, themeVariables, look
    std::optional<std::string> theme, font_family, look;
    std::map<std::string, std::string> theme_variables;
    // keys the config knows and this renderer does not draw yet (the font families and weights; a `fontSize` that is not whole px in 6 to 48): rendering throws
    std::vector<std::string> unsupported;
};

struct Model {
    std::string title, acc_title, acc_descr;
    std::vector<Actor> actors;  // in order of first appearance
    std::vector<Box> boxes;
    std::vector<Message> messages;
    std::map<std::string, size_t> created_actors;    // `create participant X`: index of the message that creates it
    std::map<std::string, size_t> destroyed_actors;  // `destroy X`
    Config config;

    const Actor* find_actor(std::string_view key) const;
    Actor* find_actor(std::string_view key);
};

}  // namespace mermaid::sequence
