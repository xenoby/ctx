#include "xml_serializer.hpp"
#include "../third_party/tinyxml2/tinyxml2.h"
#include "../core/utils.hpp"
#include <iostream>

using namespace tinyxml2;

bool XmlSerializer::write(const ContextData& data, const std::string& path) {
    XMLDocument doc;

    XMLElement* root = doc.NewElement("project_context");
    doc.InsertFirstChild(root);

    // --- summary ---
    XMLElement* summary = doc.NewElement("summary");
    root->InsertEndChild(summary);

    auto add_text = [&](XMLElement* parent, const char* name, const std::string& value) {
        XMLElement* el = doc.NewElement(name);
        el->SetText(value.c_str());
        parent->InsertEndChild(el);
    };
    auto add_int = [&](XMLElement* parent, const char* name, int64_t value) {
        XMLElement* el = doc.NewElement(name);
        el->SetText(value);
        parent->InsertEndChild(el);
    };

    add_text(summary, "mode", data.mode);
    add_int(summary, "revision", data.revision);
    if (data.mode == "delta") {
        add_int(summary, "parent_revision", data.parent_revision);
        add_int(summary, "base_revision", data.base_revision);
    }
    add_text(summary, "root", data.root);
    add_int(summary, "total_files", static_cast<long long>(data.total_files));
    add_int(summary, "included_files", static_cast<long long>(data.included_files));
    if (data.mode == "delta")
        add_int(summary, "deleted_files", static_cast<long long>(data.deleted_files));
    add_int(summary, "total_size_kb", static_cast<long long>(data.total_size_kb));
    add_text(summary, "hash_algorithm", data.hash_algorithm);
    if (!data.entry.empty())
        add_text(summary, "entry", data.entry);
    if (data.headers_only)
        add_text(summary, "headers_only", "true");

    // --- languages ---
    XMLElement* languages = doc.NewElement("languages");
    root->InsertEndChild(languages);
    for (const auto& kv : data.languages) {
        XMLElement* lang = doc.NewElement("lang");
        lang->SetAttribute("name", kv.first.c_str());
        lang->SetAttribute("count", kv.second);
        languages->InsertEndChild(lang);
    }

    // --- changes ---
    XMLElement* changes = doc.NewElement("changes");
    root->InsertEndChild(changes);
    for (const auto& p : data.added) {
        XMLElement* el = doc.NewElement("added");
        el->SetAttribute("path", p.c_str());
        changes->InsertEndChild(el);
    }
    for (const auto& p : data.modified) {
        XMLElement* el = doc.NewElement("modified");
        el->SetAttribute("path", p.c_str());
        changes->InsertEndChild(el);
    }
    for (const auto& p : data.deleted) {
        XMLElement* el = doc.NewElement("deleted");
        el->SetAttribute("path", p.c_str());
        changes->InsertEndChild(el);
    }

    // --- hash_manifest ---
    XMLElement* manifest = doc.NewElement("hash_manifest");
    root->InsertEndChild(manifest);
    for (const auto& fi : data.all_files) {
        XMLElement* f = doc.NewElement("file");
        f->SetAttribute("path", fi.rel.c_str());
        f->SetAttribute("hash_sum", fi.hash.c_str());
        manifest->InsertEndChild(f);
    }

    // --- project_structure ---
    XMLElement* structure = doc.NewElement("project_structure");
    root->InsertEndChild(structure);
    {
        std::string text;
        for (const auto& fi : data.all_files) {
            text += fi.rel;
            text += '\n';
        }
        structure->SetText(text.c_str());
    }

    // --- files ---
    XMLElement* files = doc.NewElement("files");
    root->InsertEndChild(files);

    for (const auto& of : data.files_to_write) {
        const FileInfo* fi = of.info;
        XMLElement* f = doc.NewElement("file");
        f->SetAttribute("path", fi->rel.c_str());
        f->SetAttribute("hash_sum", fi->hash.c_str());
        f->SetAttribute("lang", lang_to_string(fi->lang).c_str());
        if (data.mode == "delta")
            f->SetAttribute("status", of.status.c_str());

        if (fi->binary) {
            f->SetText("[binary file skipped]");
        } else if (!of.content_override.empty()) {
            f->SetText(of.content_override.c_str());
        } else {
            std::string content;
            if (read_file_content(fi->abs, content)) {
                f->SetText(content.c_str());
            } else {
                f->SetText("[read error]");
            }
        }
        files->InsertEndChild(f);
    }

    // --- deleted (legacy section, still useful) ---
    if (!data.deleted.empty()) {
        XMLElement* del = doc.NewElement("deleted");
        root->InsertEndChild(del);
        for (const auto& d : data.deleted) {
            XMLElement* f = doc.NewElement("file");
            f->SetAttribute("path", d.c_str());
            del->InsertEndChild(f);
        }
    }

    XMLError err = doc.SaveFile(path.c_str());
    if (err != XML_SUCCESS) {
        std::cerr << "XML save error: " << err << "\n";
        return false;
    }
    return true;
}
