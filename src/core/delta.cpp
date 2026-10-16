#include "delta.hpp"
#include <fstream>
#include <algorithm>

static std::string extract_attr(const std::string& line, const std::string& name) {
    std::string key = name + "=\"";
    size_t pos = line.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();
    size_t end = line.find('"', pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}

std::unordered_map<std::string, std::string> parse_manifest_xml(const fs::path& path) {
    std::unordered_map<std::string, std::string> result;
    std::ifstream in(path);
    if (!in) return result;

    std::string line;
    bool in_manifest = false;
    while (std::getline(in, line)) {
        if (line.find("<hash_manifest>") != std::string::npos) { in_manifest = true; continue; }
        if (line.find("</hash_manifest>") != std::string::npos) { in_manifest = false; continue; }
        if (!in_manifest) continue;

        std::string p = extract_attr(line, "path");
        std::string h = extract_attr(line, "hash_sum");
        if (!p.empty() && !h.empty()) result[p] = h;
    }
    return result;
}

std::unordered_map<std::string, std::string> load_manifest(const fs::path& path) {
    std::unordered_map<std::string, std::string> result;
    std::ifstream in(path);
    if (!in) return result;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        result[line.substr(0, tab)] = line.substr(tab + 1);
    }
    return result;
}

void save_manifest(const fs::path& path, const std::vector<FileInfo>& files) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) return;
    for (const auto& fi : files)
        out << fi.rel << '\t' << fi.hash << '\n';
}

DeltaResult compute_delta(const std::vector<FileInfo*>& candidates,
                          const std::vector<FileInfo>& all_files,
                          const std::unordered_map<std::string, std::string>& prev_manifest,
                          bool have_prev) {
    DeltaResult dr;
    dr.have_prev = have_prev;

    std::unordered_set<std::string> current_rel_set;
    for (const auto& fi : all_files)
        current_rel_set.insert(fi.rel);

    if (have_prev) {
        for (const auto* fi : candidates) {
            auto it = prev_manifest.find(fi->rel);
            if (it == prev_manifest.end()) {
                dr.files_to_write.push_back({fi, "new"});
                dr.added.push_back(fi->rel);
            } else if (it->second != fi->hash) {
                dr.files_to_write.push_back({fi, "modified"});
                dr.modified.push_back(fi->rel);
            }
        }
        for (const auto& kv : prev_manifest) {
            if (!current_rel_set.count(kv.first))
                dr.deleted.push_back(kv.first);
        }
        std::sort(dr.deleted.begin(), dr.deleted.end());
    } else {
        for (const auto* fi : candidates) {
            dr.files_to_write.push_back({fi, "full"});
            dr.added.push_back(fi->rel);
        }
    }
    return dr;
}
