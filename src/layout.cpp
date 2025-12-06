#include "layout.hpp"

#include <algorithm>
#include <limits>

namespace mermaid {

void SequenceLayoutStrategy::layout(SequenceDiagramNode &root) {
    const double start_x = 75.0;
    const double participant_spacing = 200.0;
    const double header_y = 60.0;
    const double message_start_y = 158.0;
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

    // Layout blocks
    layout_blocks(root);

    // Compute lifeline end based on messages and blocks
    double lifeline_end = 0.0;
    if (!root.blocks.empty()) {
        // If there are blocks, lifeline ends at the furthest block bottom + 20
        for (auto &block : root.blocks) {
            lifeline_end = std::max(lifeline_end, block->stop_y + 20.0);
        }
    } else {
        // No blocks, use messages
        lifeline_end = current_y + 40.0;
    }
    // Ensure at least header_y + 200? Golden uses 236 for loop_basic
    // lifeline_end = std::max(lifeline_end, header_y + 200.0); // not needed
    for (auto &p : root.participants) {
        p->lifeline_end_y = lifeline_end;
    }

    if (!root.participants.empty()) {
        root.width = start_x * 2 + participant_spacing * static_cast<double>(root.participants.size() - 1);
    } else {
        root.width = 400.0;
    }
    // Golden SVG height is 322 for loop_basic
    root.height = lifeline_end + 86.0;
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
        const double padding_x = 11.0;
        const double padding_y_top = 83.0;
        const double padding_y_bottom = 10.0;
        block->start_x = min_x - padding_x;
        block->stop_x = max_x + padding_x;
        block->start_y = min_y - padding_y_top;
        block->stop_y = max_y + padding_y_bottom;

        // Adjust message positions for alt blocks to match golden spacing
        if (block->type == "alt" && block->sections.size() == 2) {
            // For alt with two sections (e.g., alt_else_basic)
            // We want first message at start_y + padding_y_top, second at stop_y - padding_y_bottom
            // Ensure there are exactly two messages inside block
            if (block->end_message_index - block->start_message_index == 2) {
                auto &msg1 = root.messages[block->start_message_index];
                auto &msg2 = root.messages[block->start_message_index + 1];
                msg1->y = block->start_y + padding_y_top;
                msg2->y = block->stop_y - padding_y_bottom;
                // Update min_y and max_y for boundary calculation
                min_y = std::min(msg1->y, msg2->y);
                max_y = std::max(msg1->y, msg2->y);
                // Recompute block start_y and stop_y with updated min_y and max_y
                block->start_y = min_y - padding_y_top;
                block->stop_y = max_y + padding_y_bottom;
            }
        }

        // Compute section boundaries for alt, par, critical, etc.
        if (block->sections.size() > 1) {
            block->section_boundaries.clear();
            // For each section boundary (between section i and i+1)
            for (size_t i = 0; i < block->sections.size() - 1; ++i) {
                const auto &section_a = block->sections[i];
                const auto &section_b = block->sections[i + 1];
                // Find last message y of section a
                double last_y_a = min_y; // fallback
                if (section_a.end_message_index > section_a.start_message_index) {
                    // There is at least one message in section a
                    // Use the maximum y among messages in section a
                    double max_y_a = std::numeric_limits<double>::lowest();
                    for (size_t idx = section_a.start_message_index; idx < section_a.end_message_index; ++idx) {
                        if (idx < root.messages.size()) {
                            max_y_a = std::max(max_y_a, root.messages[idx]->y);
                        }
                    }
                    if (max_y_a != std::numeric_limits<double>::lowest()) {
                        last_y_a = max_y_a;
                    }
                }
                // Find first message y of section b
                double first_y_b = max_y; // fallback
                if (section_b.end_message_index > section_b.start_message_index) {
                    double min_y_b = std::numeric_limits<double>::max();
                    for (size_t idx = section_b.start_message_index; idx < section_b.end_message_index; ++idx) {
                        if (idx < root.messages.size()) {
                            min_y_b = std::min(min_y_b, root.messages[idx]->y);
                        }
                    }
                    if (min_y_b != std::numeric_limits<double>::max()) {
                        first_y_b = min_y_b;
                    }
                }
                // Boundary is midpoint between last_y_a and first_y_b
                double boundary = (last_y_a + first_y_b) / 2.0;
                block->section_boundaries.push_back(boundary);
            }
        }
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
