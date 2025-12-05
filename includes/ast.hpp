#pragma once

#include <memory>
#include <string>
#include <vector>

namespace mermaid {

class AstVisitor;

struct AstNode {
    virtual ~AstNode() = default;
    virtual void accept(AstVisitor &v) = 0;
};

struct ParticipantNode : AstNode {
    std::string id;
    std::string label;
    std::string type; // "participant" or "actor"
    double x{0.0};
    double y{0.0};
    double lifeline_end_y{0.0};

    ParticipantNode(std::string id_, std::string label_, std::string type_ = "participant")
        : id(std::move(id_)), label(std::move(label_)), type(std::move(type_)) {}

    void accept(AstVisitor &v) override;
};

struct NoteNode : AstNode {
    std::string actor;
    std::string placement; // "left of", "right of", "over"
    std::string text;
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};

    NoteNode(std::string actor_, std::string placement_, std::string text_)
        : actor(std::move(actor_)), placement(std::move(placement_)), text(std::move(text_)) {}

    void accept(AstVisitor &v) override;
};

struct MessageNode : AstNode {
    std::string from;
    std::string to;
    std::string text;
    double y{0.0};
    double from_x{0.0};
    double to_x{0.0};
    bool activate_target{false};  // + after arrow
    bool deactivate_source{false}; // - after arrow
    bool is_dotted{false};        // --> or -->> vs -> or ->>
    bool has_cross{false};        // ->> or -->> vs -> or -->

    MessageNode(std::string from_, std::string to_, std::string text_)
        : from(std::move(from_)), to(std::move(to_)), text(std::move(text_)) {}

    void accept(AstVisitor &v) override;
};

struct BlockNode : AstNode {
    std::string type; // "loop", "alt", "opt", "par", "break", "critical", "rect"
    std::string label;
    double start_y{0.0};
    double end_y{0.0};
    std::vector<std::unique_ptr<MessageNode>> messages; // messages inside block

    BlockNode(std::string type_, std::string label_)
        : type(std::move(type_)), label(std::move(label_)) {}

    void accept(AstVisitor &v) override;
};

struct SequenceDiagramNode : AstNode {
    std::vector<std::unique_ptr<ParticipantNode>> participants;
    std::vector<std::unique_ptr<MessageNode>> messages;
    std::vector<std::unique_ptr<NoteNode>> notes;
    std::vector<std::unique_ptr<BlockNode>> blocks;
    double width{800.0};
    double height{600.0};

    void accept(AstVisitor &v) override;
};

} // namespace mermaid
