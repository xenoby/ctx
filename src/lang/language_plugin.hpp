#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "../core/types.hpp"

namespace fs = std::filesystem;

/**
 * Language plugin interface (level 0–1 for now).
 *
 * Level 0: language detection + interface-file recognition
 * Level 1: import parsing + path resolution
 *
 * Future levels (symbols, calls, etc.) can be added without
 * breaking existing plugins.
 */
class ILanguagePlugin {
public:
    virtual ~ILanguagePlugin() = default;

    virtual std::string id() const = 0;

    virtual bool supports(const fs::path& path) const = 0;

    /** Header / declaration / interface files (.h, .pyi, .d.ts, ...) */
    virtual bool is_interface_file(const fs::path& path) const = 0;

    virtual std::vector<std::string> parse_imports(const std::string& content) const = 0;

    virtual fs::path resolve_import(
        const std::string& imp,
        const fs::path& cur_dir,
        const fs::path& root) const = 0;
};
