#include "dagre_acyclic.h"

#include <functional>
#include <unordered_set>

namespace mermaid::dagre::detail {

void Acyclic::run() {
    std::vector<EdgeObj> back_edges;
    std::unordered_set<std::string> on_stack;
    std::unordered_set<std::string> visited;

    std::function<void(const std::string&)> visit = [&](const std::string& v) {
        if (!visited.insert(v).second) return;
        on_stack.insert(v);
        for (const EdgeObj& e : g_.out_edges(v)) {
            if (on_stack.count(e.w))
                back_edges.push_back(e);
            else
                visit(e.w);
        }
        on_stack.erase(v);
    };
    for (const std::string& v : g_.nodes()) visit(v);

    for (const EdgeObj& e : back_edges) {
        const EdgeLabelPtr label = g_.edge(e);
        g_.remove_edge(e);
        label->forward_name = e.name;
        label->reversed = true;
        g_.set_edge(e.w, e.v, label, ids_.next("rev"));
    }
}

void Acyclic::undo() {
    for (const EdgeObj& e : g_.edges()) {
        const EdgeLabelPtr label = g_.edge(e);
        if (!label->reversed) continue;
        g_.remove_edge(e);
        const std::string name = label->forward_name;
        label->reversed = false;
        label->forward_name.clear();
        g_.set_edge(e.w, e.v, label, name);
    }
}

}  // namespace mermaid::dagre::detail
