#include <utility>
#include <mermaid/sequence.h>

namespace mermaid::sequence {

const Actor* Model::find_actor(std::string_view key) const {
    for (const Actor& actor : actors)
        if (actor.key == key) return &actor;
    return nullptr;
}

Actor* Model::find_actor(std::string_view key) {
    return const_cast<Actor*>(std::as_const(*this).find_actor(key));
}

}  // namespace mermaid::sequence
