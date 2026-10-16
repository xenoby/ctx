#include "blob_store.hpp"
#include "utils.hpp"
#include "revision.hpp"
#include <fstream>
#include <iostream>
#include <unordered_set>

BlobStore::BlobStore(const fs::path& root)
    : root_(root)
    , objects_dir_(ctx_dir(root) / "objects")
{}

fs::path BlobStore::blob_path(const std::string& hash) const {
    if (hash.size() < 2) return objects_dir_ / hash;
    return objects_dir_ / hash.substr(0, 2) / hash;
}

bool BlobStore::exists(const std::string& hash) const {
    std::error_code ec;
    return fs::is_regular_file(blob_path(hash), ec);
}

std::string BlobStore::store_content(const std::string& content, const std::string& known_hash) {
    std::string hash = known_hash;
    if (hash.empty()) {
        uint64_t h = 14695981039346656037ULL;
        for (unsigned char c : content) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        hash = fnv1a_hex(h);
    }

    fs::path dest = blob_path(hash);
    std::error_code ec;
    if (fs::is_regular_file(dest, ec)) return hash;

    fs::create_directories(dest.parent_path(), ec);
    std::ofstream out(dest, std::ios::binary);
    if (!out) {
        std::cerr << "[WARN] cannot write blob " << hash << "\n";
        return hash;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return hash;
}

std::string BlobStore::store_file(const fs::path& abs_path, const std::string& known_hash) {
    std::string content;
    if (!read_file_content(abs_path, content)) {
        std::cerr << "[WARN] cannot read for blob: " << abs_path << "\n";
        return known_hash;
    }
    return store_content(content, known_hash);
}

std::string BlobStore::load(const std::string& hash) const {
    std::string content;
    if (!read_file_content(blob_path(hash), content)) return {};
    return content;
}

void BlobStore::store_changed(const std::vector<FileInfo>& all_files,
                              const std::vector<std::string>& added,
                              const std::vector<std::string>& modified) {
    std::unordered_set<std::string> need;
    for (const auto& p : added) need.insert(p);
    for (const auto& p : modified) need.insert(p);

    const bool store_all = need.empty() && !all_files.empty();

    for (const auto& fi : all_files) {
        if (fi.binary) continue;
        if (store_all || need.count(fi.rel)) {
            if (!exists(fi.hash))
                store_file(fi.abs, fi.hash);
        }
    }
}

static fs::path rev_manifest_path(const fs::path& root, int rev) {
    return ctx_dir(root) / "revs" / (std::to_string(rev) + ".manifest");
}

void save_rev_manifest(const fs::path& root, int rev, const std::vector<FileInfo>& files) {
    fs::path p = rev_manifest_path(root, rev);
    fs::create_directories(p.parent_path());
    std::ofstream out(p);
    if (!out) return;
    for (const auto& fi : files)
        out << fi.rel << '\t' << fi.hash << '\n';
}

RevManifest load_rev_manifest(const fs::path& root, int rev) {
    RevManifest result;
    std::ifstream in(rev_manifest_path(root, rev));
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

bool rev_manifest_exists(const fs::path& root, int rev) {
    std::error_code ec;
    return fs::is_regular_file(rev_manifest_path(root, rev), ec);
}
