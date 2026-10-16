#include "revision.hpp"
#include <fstream>
#include <iostream>

fs::path ctx_dir(const fs::path& root) {
    return root / ".ctx";
}

bool is_initialized(const fs::path& root) {
    return fs::is_directory(ctx_dir(root));
}

RevisionInfo read_revision(const fs::path& root) {
    RevisionInfo info;
    fs::path p = ctx_dir(root) / "revision";
    std::ifstream in(p);
    if (in) in >> info.current;
    info.parent = info.current;
    return info;
}

void write_revision(const fs::path& root, int rev) {
    auto dir = ctx_dir(root);
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path p = dir / "revision";
    std::ofstream out(p, std::ios::trunc);
    if (!out) {
        std::cerr << "[WARN] cannot write revision file\n";
        return;
    }
    out << rev << '\n';
    out.flush();
    out.close();
}

void append_history(const fs::path& root,
                    int rev, int parent,
                    const std::vector<std::string>& modified,
                    const std::vector<std::string>& added,
                    const std::vector<std::string>& deleted,
                    const std::string& reason) {
    auto dir = ctx_dir(root);
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::ofstream out(dir / "history.jsonl", std::ios::app);
    if (!out) return;

    auto write_list = [&](const std::vector<std::string>& list) {
        out << "[";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i) out << ",";
            out << "\"" << list[i] << "\"";
        }
        out << "]";
    };

    out << "{\"rev\":" << rev << ",\"parent\":" << parent;
    out << ",\"modified\":"; write_list(modified);
    out << ",\"added\":"; write_list(added);
    out << ",\"deleted\":"; write_list(deleted);
    if (!reason.empty()) {
        std::string r = reason;
        for (char& c : r) if (c == '"') c = '\'';
        out << ",\"reason\":\"" << r << "\"";
    }
    out << "}\n";
    out.flush();
}

std::vector<std::string> read_history_lines(const fs::path& root) {
    std::vector<std::string> lines;
    std::ifstream in(ctx_dir(root) / "history.jsonl");
    if (!in) return lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}
