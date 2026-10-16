#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include "file_info.hpp"

namespace fs = std::filesystem;

inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::istringstream stream(s);
    std::string item;
    while (std::getline(stream, item, delim))
        result.push_back(item);
    return result;
}

inline bool ends_with(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string normalize_ws(std::string s) {
    std::replace(s.begin(), s.end(), '\t', ' ');
    return s;
}

inline std::string fnv1a_hex(uint64_t h) {
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << h;
    return ss.str();
}

inline FileHashResult hash_file(const fs::path& p) {
    std::error_code ec;
    auto sz = fs::file_size(p, ec);
    if (ec) return {};

    std::ifstream in(p, std::ios::binary);
    if (!in) return {};

    uint64_t h = 14695981039346656037ULL;
    bool binary = false;
    size_t total = 0;
    char buf[8192];

    while (in.read(buf, sizeof(buf)) || in.gcount() > 0) {
        std::streamsize n = in.gcount();
        for (std::streamsize i = 0; i < n; ++i) {
            unsigned char c = static_cast<unsigned char>(buf[i]);
            h ^= c;
            h *= 1099511628211ULL;
            if (total + static_cast<size_t>(i) < 8192 && c == '\0')
                binary = true;
        }
        total += static_cast<size_t>(n);
    }
    return { h, sz, binary, true };
}

inline bool read_file_content(const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}
