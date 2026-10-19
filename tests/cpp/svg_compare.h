// svg_compare.h - structural comparison of two SVG documents (tests only).
//
// Two documents are equal when they have the same element tree: same element names in the same
// order, the same attribute names, the same text, and attribute values that are equal after
// splitting them into numbers and other text, where numbers may differ by `tolerance`.
// Whitespace-only text, comments and the XML declaration are ignored.
#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <pugixml.hpp>

namespace svgtest {

class svg_comparer {
public:
    explicit svg_comparer(double tolerance = 0.5) : tolerance_(tolerance) {}

    // Compares only the attributes of matching elements named like `selector` (their children are
    // skipped). A selector is an element name (`style`) or `name.class` (`g.root`). Used to check
    // one part of the document while the rest is still being built.
    svg_comparer& prune(std::string selector) {
        pruned_.push_back(std::move(selector));
        return *this;
    }

    // Skips the value (not the presence) of every attribute called `name`. Used where the value
    // depends on the layout engine (path geometry) while the rest of the markup must match.
    svg_comparer& ignore_attribute(std::string name) {
        ignored_attributes_.push_back(std::move(name));
        return *this;
    }

    // Compares only the first element matching `selector` (see prune) of each document, with
    // everything below it.
    std::vector<std::string> compare_within(const std::string& selector, const std::string& expected,
                                            const std::string& actual) const {
        pugi::xml_document a, b;
        if (!a.load_string(expected.c_str())) return {"expected is not valid XML"};
        if (!b.load_string(actual.c_str())) return {"actual is not valid XML"};
        const pugi::xml_node x = find(a.document_element(), selector), y = find(b.document_element(), selector);
        if (!x) return {"expected has no <" + selector + ">"};
        if (!y) return {"actual has no <" + selector + ">"};
        std::vector<std::string> out;
        compare_node(x, y, "/" + selector, out);
        return out;
    }

    // Returns one line per difference; empty means the documents match.
    std::vector<std::string> compare(const std::string& expected, const std::string& actual) const {
        std::vector<std::string> out;
        pugi::xml_document a, b;
        if (!a.load_string(expected.c_str())) return {"expected is not valid XML"};
        if (!b.load_string(actual.c_str())) return {"actual is not valid XML"};
        compare_node(a.document_element(), b.document_element(), "/" + std::string(a.document_element().name()), out);
        return out;
    }

private:
    struct Token {
        bool is_number = false;
        double number = 0;
        std::string text;
    };

    static std::vector<Token> tokenize(const std::string& s) {
        std::vector<Token> tokens;
        size_t i = 0;
        std::string text;
        auto flush = [&] {
            if (!text.empty()) {
                Token t;
                t.text = text;
                tokens.push_back(t);
                text.clear();
            }
        };
        while (i < s.size()) {
            const char c = s[i];
            const bool digit = c >= '0' && c <= '9';
            const bool sign_start = (c == '-' || c == '+' || c == '.') && i + 1 < s.size() &&
                                    ((s[i + 1] >= '0' && s[i + 1] <= '9') || (c != '.' && s[i + 1] == '.'));
            if (digit || sign_start) {
                char* end = nullptr;
                const double v = std::strtod(s.c_str() + i, &end);
                const size_t len = static_cast<size_t>(end - (s.c_str() + i));
                if (len > 0) {
                    flush();
                    Token t;
                    t.is_number = true;
                    t.number = v;
                    tokens.push_back(t);
                    i += len;
                    continue;
                }
            }
            text += c;
            ++i;
        }
        flush();
        return tokens;
    }

    bool values_match(const std::string& x, const std::string& y) const {
        if (x == y) return true;
        const std::vector<Token> a = tokenize(x), b = tokenize(y);
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i].is_number != b[i].is_number) return false;
            if (a[i].is_number ? std::fabs(a[i].number - b[i].number) > tolerance_ : a[i].text != b[i].text)
                return false;
        }
        return true;
    }

    // Both values around the first position where they differ (they can be long).
    static std::string first_difference(const std::string& x, const std::string& y) {
        size_t i = 0;
        while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
        const size_t from = i > 40 ? i - 40 : 0;
        return "\"..." + x.substr(from, 100) + "\" vs \"..." + y.substr(from, 100) + "\"";
    }

    static std::vector<pugi::xml_node> significant_children(const pugi::xml_node& n) {
        std::vector<pugi::xml_node> out;
        for (pugi::xml_node c : n.children()) {
            if (c.type() == pugi::node_element) out.push_back(c);
            else if (c.type() == pugi::node_pcdata || c.type() == pugi::node_cdata) {
                std::string t = c.value();
                if (t.find_first_not_of(" \t\r\n") != std::string::npos) out.push_back(c);
            }
        }
        return out;
    }

    static bool matches(const pugi::xml_node& n, const std::string& sel) {
        const size_t dot = sel.find('.');
        if (std::string(n.name()) != sel.substr(0, dot)) return false;
        if (dot == std::string::npos) return true;
        const std::string cls = " " + std::string(n.attribute("class").value()) + " ";
        return cls.find(" " + sel.substr(dot + 1) + " ") != std::string::npos;
    }

    static pugi::xml_node find(const pugi::xml_node& n, const std::string& sel) {
        if (matches(n, sel)) return n;
        for (pugi::xml_node c : n.children()) {
            if (c.type() != pugi::node_element) continue;
            if (pugi::xml_node hit = find(c, sel)) return hit;
        }
        return {};
    }

    bool is_ignored(const std::string& attribute) const {
        for (const std::string& n : ignored_attributes_)
            if (n == attribute) return true;
        return false;
    }

    bool is_pruned(const pugi::xml_node& n) const {
        for (const std::string& sel : pruned_)
            if (matches(n, sel)) return true;
        return false;
    }

    void compare_node(const pugi::xml_node& a, const pugi::xml_node& b, const std::string& path,
                      std::vector<std::string>& out) const {
        if (a.type() != b.type()) {
            out.push_back(path + ": node kind differs");
            return;
        }
        if (a.type() != pugi::node_element) {
            if (!values_match(a.value(), b.value())) out.push_back(path + ": text " + first_difference(a.value(), b.value()));
            return;
        }
        if (std::string(a.name()) != b.name()) {
            out.push_back(path + ": element <" + a.name() + "> vs <" + b.name() + ">");
            return;
        }
        for (pugi::xml_attribute at : a.attributes()) {
            pugi::xml_attribute other = b.attribute(at.name());
            if (!other) out.push_back(path + ": attribute " + at.name() + " missing");
            else if (!is_ignored(at.name()) && !values_match(at.value(), other.value()))
                out.push_back(path + "@" + at.name() + ": " + first_difference(at.value(), other.value()));
        }
        for (pugi::xml_attribute at : b.attributes())
            if (!a.attribute(at.name())) out.push_back(path + ": unexpected attribute " + at.name());

        if (is_pruned(a)) return;
        const std::vector<pugi::xml_node> ca = significant_children(a), cb = significant_children(b);
        if (ca.size() != cb.size())
            out.push_back(path + ": " + std::to_string(ca.size()) + " children vs " + std::to_string(cb.size()));
        const size_t n = ca.size() < cb.size() ? ca.size() : cb.size();
        for (size_t i = 0; i < n; ++i) {
            std::string label = ca[i].type() == pugi::node_element ? ca[i].name() : "#text";
            compare_node(ca[i], cb[i], path + "/" + label + "[" + std::to_string(i) + "]", out);
        }
    }

    double tolerance_;
    std::vector<std::string> pruned_;
    std::vector<std::string> ignored_attributes_;
};

}  // namespace svgtest
