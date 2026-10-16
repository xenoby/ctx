#pragma once

#include <memory>
#include <vector>
#include <filesystem>
#include "language_plugin.hpp"
#include "../core/types.hpp"

namespace fs = std::filesystem;

class PluginRegistry {
public:
    PluginRegistry();

    const ILanguagePlugin* find(const fs::path& path) const;
    Lang detect_lang(const fs::path& path) const;
    bool is_supported(const fs::path& path) const;
    bool is_interface_file(const fs::path& path) const;

    const std::vector<std::unique_ptr<ILanguagePlugin>>& plugins() const { return plugins_; }

private:
    std::vector<std::unique_ptr<ILanguagePlugin>> plugins_;
};
