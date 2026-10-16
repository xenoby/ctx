#pragma once

#include "language_plugin.hpp"

class KotlinPlugin : public ILanguagePlugin {
public:
    std::string id() const override { return "kotlin"; }
    bool supports(const fs::path& path) const override;
    bool is_interface_file(const fs::path& path) const override;
    std::vector<std::string> parse_imports(const std::string& content) const override;
    fs::path resolve_import(const std::string& imp, const fs::path& cur_dir, const fs::path& root) const override;
};
