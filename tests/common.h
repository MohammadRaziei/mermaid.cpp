#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace test_utils {

inline void trim(std::string &s) {
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

inline std::string normalize(const std::string &input) {
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

inline std::string load_file_optional(const std::filesystem::path &p) {
    std::ifstream ifs(p);
    if (!ifs.good()) {
        return {};
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

inline std::string load_file_required(const std::filesystem::path &p) {
    auto data = load_file_optional(p);
    if (data.empty()) {
        throw std::runtime_error("Failed to read file: " + p.string());
    }
    return data;
}

} // namespace test_utils

