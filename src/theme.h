// theme.h (private) - colours and font of a diagram theme.
#pragma once

#include <string>
#include <string_view>

namespace mermaid::detail {

struct Theme {
    std::string name;
    std::string font_family = "\"trebuchet ms\", verdana, arial, sans-serif";
    double font_size = 14;
    std::string node_fill;
    std::string node_border;
    std::string text;
    std::string line;
    std::string cluster_fill;
    std::string cluster_border;
    std::string edge_label_bg;

    // default | dark | forest | neutral. Throws std::invalid_argument otherwise.
    static Theme named(std::string_view name);
};

}  // namespace mermaid::detail
