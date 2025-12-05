#include "layout.hpp"

#include <algorithm>
#include <limits>

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

    // Layout blocks
    layout_blocks(root);

    if (!root.participants.empty()) {
        root.width = start_x * 2 + participant_spacing * static_cast<double>(root.participants.size() - 1);
    } else {
        root.width = 400.0;
    }
    root.height = lifeline_end + 40.0;
}

void SequenceLayoutStrategy::layout_blocks(SequenceDiagramNode &root) {
    for (auto &block : root.blocks) {
        if (block->start_message_index >= block->end_message_index) {
            // No messages inside block, skip
            continue;
        }
        // Find min and max x among participants (or messages)
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        
        // Consider participants' x positions
        for (auto &p : root.participants) {
            min_x = std::min(min_x, p->x);
            max_x = std::max(max_x, p->x);
        }
        
        // Consider messages inside block
        for (size_t i = block->start_message_index; i < block->end_message_index; ++i) {
            auto &msg = root.messages[i];
            min_x = std::min(min_x, std::min(msg->from_x, msg->to_x));
            max_x = std::max(max_x, std::max(msg->from_x, msg->to_x));
            min_y = std::min(min_y, msg->y);
            max_y = std::max(max_y, msg->y);
        }
        
        // Add padding
        const double padding_x = 10.0;
        const double padding_y_top = 50.0;
        const double padding_y_bottom = 10.0;
        block->start_x = min_x - padding_x;
        block->stop_x = max_x + padding_x;
        block->start_y = min_y - padding_y_top;
        block->stop_y = max_y + padding_y_bottom;
    }
}

std::unordered_map<std::string, ParticipantNode *> SequenceLayoutStrategy::build_participant_map(SequenceDiagramNode &root) {
    std::unordered_map<std::string, ParticipantNode *> map;
    for (auto &p : root.participants) {
        map[p->id] = p.get();
    }
    return map;
}

} // namespace mermaid
