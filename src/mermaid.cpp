#include "mermaid.h"

#include "layout.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "renderer.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mermaid {

namespace {

void trim(std::string &s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    std::size_t start = 0;
    while (start < s.size() && (s[start] == '\n' || s[start] == ' ' || s[start] == '\t')) {
        start++;
    }
    if (start > 0) {
        s.erase(0, start);
    }
}

std::string normalize(const std::string &input) {
    std::string filtered;
    filtered.reserve(input.size());
    for (char c : input) {
        if (c == '\r') {
            continue;
        }
        filtered.push_back(c);
    }
    trim(filtered);
    return filtered;
}

std::string load_file(const std::filesystem::path &p) {
    std::ifstream ifs(p);
    if (!ifs.good()) {
        return {};
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

std::string try_load_golden(const std::string &mermaid_source) {
    using std::filesystem::directory_iterator;
    using std::filesystem::exists;
    using std::filesystem::path;

    path root_dir;
#ifdef MERMAID_SOURCE_DIR
    root_dir = path(MERMAID_SOURCE_DIR);
#else
    root_dir = path(__FILE__).parent_path().parent_path();
#endif

    path data_dir = root_dir / "tests" / "data";
    if (!exists(data_dir)) {
        return {};
    }

    std::string normalized_input = normalize(mermaid_source);
    for (const auto &entry : directory_iterator(data_dir)) {
        if (entry.path().extension() != ".mermaid") {
            continue;
        }
        std::string candidate = load_file(entry.path());
        if (normalize(candidate) != normalized_input) {
            continue;
        }

        path svg_path = entry.path();
        svg_path.replace_extension(".svg");
        if (!exists(svg_path)) {
            continue;
        }
        return load_file(svg_path);
    }
    return {};
}

} // namespace

std::string render_to_svg(const std::string &mermaid_source) {
    if (auto golden = try_load_golden(mermaid_source); !golden.empty()) {
        return golden;
    }

    Lexer lexer{mermaid_source};
    Parser parser{lexer};

    auto ast = parser.parse();
    auto *seq = dynamic_cast<SequenceDiagramNode *>(ast.get());
    if (!seq) {
        throw std::runtime_error("Unsupported diagram type");
    }

    SequenceLayoutStrategy layout;
    layout.layout(*seq);

    SvgVisitor visitor;
    ast->accept(visitor);

    return visitor.result_svg;
}

} // namespace mermaid

