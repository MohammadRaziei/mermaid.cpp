#pragma once

#include "ast.hpp"
#include <unordered_map>

namespace mermaid {

class LayoutStrategy {
public:
    virtual ~LayoutStrategy() = default;
    virtual void layout(SequenceDiagramNode &root) = 0;
};

class SequenceLayoutStrategy : public LayoutStrategy {
public:
    void layout(SequenceDiagramNode &root) override;

private:
    std::unordered_map<std::string, ParticipantNode *> build_participant_map(SequenceDiagramNode &root);
    void layout_blocks(SequenceDiagramNode &root);
};

} // namespace mermaid
