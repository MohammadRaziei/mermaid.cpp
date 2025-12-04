#include "layout.hpp"

#include <algorithm>

namespace mermaid {

void SequenceLayoutStrategy::layout(SequenceDiagramNode &root) {
    const double start_x = 75.0;
    const double participant_spacing = 200.0;
    const double header_y = 60.0;
    const double message_start_y = 113.0;
    const double message_gap = 48.0;

    auto participant_map = build_participant_map(root);

    for (std::size_t i = 0; i < root.participants.size(); ++i) {
        auto &p = root.participants[i];
        p->x = start_x + participant_spacing * static_cast<double>(i);
        p->y = header_y;
    }

    double current_y = message_start_y;
    for (auto &msg : root.messages) {
        msg->y = current_y;
        auto from_it = participant_map.find(msg->from);
        auto to_it = participant_map.find(msg->to);
        if (from_it != participant_map.end()) {
            msg->from_x = from_it->second->x;
        }
        if (to_it != participant_map.end()) {
            msg->to_x = to_it->second->x;
        }
        current_y += message_gap;
    }

    double lifeline_end = std::max(current_y + 40.0, header_y + 200.0);
    for (auto &p : root.participants) {
        p->lifeline_end_y = lifeline_end;
    }

    if (!root.participants.empty()) {
        root.width = start_x * 2 + participant_spacing * static_cast<double>(root.participants.size() - 1);
    } else {
        root.width = 400.0;
    }
    root.height = lifeline_end + 40.0;
}

std::unordered_map<std::string, ParticipantNode *> SequenceLayoutStrategy::build_participant_map(SequenceDiagramNode &root) {
    std::unordered_map<std::string, ParticipantNode *> map;
    for (auto &p : root.participants) {
        map[p->id] = p.get();
    }
    return map;
}

} // namespace mermaid
