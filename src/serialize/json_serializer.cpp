#include "json_serializer.hpp"
#include "../third_party/nlohmann/json.hpp"
#include "../core/utils.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

bool JsonSerializer::write(const ContextData& data, const std::string& path) {
    json root;

    json summary;
    summary["mode"] = data.mode;
    summary["revision"] = data.revision;
    if (data.mode == "delta") {
        summary["parent_revision"] = data.parent_revision;
        summary["base_revision"] = data.base_revision;
    }
    summary["root"] = data.root;
    summary["total_files"] = data.total_files;
    summary["included_files"] = data.included_files;
    if (data.mode == "delta")
        summary["deleted_files"] = data.deleted_files;
    summary["total_size_kb"] = data.total_size_kb;
    summary["hash_algorithm"] = data.hash_algorithm;
    if (!data.entry.empty())
        summary["entry"] = data.entry;
    if (data.headers_only)
        summary["headers_only"] = true;
    root["summary"] = summary;

    json languages = json::object();
    for (const auto& kv : data.languages)
        languages[kv.first] = kv.second;
    root["languages"] = languages;

    json changes;
    changes["added"] = data.added;
    changes["modified"] = data.modified;
    changes["deleted"] = data.deleted;
    root["changes"] = changes;

    json manifest = json::array();
    for (const auto& fi : data.all_files) {
        manifest.push_back({
            {"path", fi.rel},
            {"hash_sum", fi.hash}
            });
    }
    root["hash_manifest"] = manifest;

    json structure = json::array();
    for (const auto& fi : data.all_files)
        structure.push_back(fi.rel);
    root["project_structure"] = structure;

    json files = json::array();
    for (const auto& of : data.files_to_write) {
        const FileInfo* fi = of.info;
        json f;
        f["path"] = fi->rel;
        f["hash_sum"] = fi->hash;
        f["lang"] = lang_to_string(fi->lang);
        if (data.mode == "delta")
            f["status"] = of.status;

        if (fi->binary) {
            f["content"] = "[binary file skipped]";
        }
        else if (!of.content_override.empty()) {
            f["content"] = of.content_override;
        }
        else {
            std::string content;
            if (read_file_content(fi->abs, content))
                f["content"] = content;
            else
                f["content"] = "[read error]";
        }
        files.push_back(std::move(f));
    }
    root["files"] = files;

    if (!data.deleted.empty())
        root["deleted"] = data.deleted;

    std::ofstream out(path);
    if (!out) {
        std::cerr << "Cannot write " << path << "\n";
        return false;
    }
    //out << root.dump(2) << "\n";

    try {
        // replace Ч невалидный UTF-8 замен€етс€ на U+FFFD, исключение не бросаетс€
        out << root.dump(2, ' ', false, json::error_handler_t::replace) << "\n";
    }
    catch (const std::exception & e) {
        std::cerr << "JSON dump error: " << e.what() << "\n";
        return false;
    }

    if (!out.good()) {
        std::cerr << "IO error writing " << path << "\n";
        return false;
    }

    return true;
}
