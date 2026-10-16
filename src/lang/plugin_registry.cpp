#include "plugin_registry.hpp"
#include "cpp_plugin.hpp"
#include "python_plugin.hpp"
#include "js_plugin.hpp"
#include "rust_plugin.hpp"
#include "go_plugin.hpp"
#include "csharp_plugin.hpp"
#include "java_plugin.hpp"
#include "kotlin_plugin.hpp"
#include "swift_plugin.hpp"

PluginRegistry::PluginRegistry() {
    plugins_.push_back(std::make_unique<CppPlugin>());
    plugins_.push_back(std::make_unique<PythonPlugin>());
    plugins_.push_back(std::make_unique<JsPlugin>());
    plugins_.push_back(std::make_unique<RustPlugin>());
    plugins_.push_back(std::make_unique<GoPlugin>());
    plugins_.push_back(std::make_unique<CSharpPlugin>());
    plugins_.push_back(std::make_unique<JavaPlugin>());
    plugins_.push_back(std::make_unique<KotlinPlugin>());
    plugins_.push_back(std::make_unique<SwiftPlugin>());
}

const ILanguagePlugin* PluginRegistry::find(const fs::path& path) const {
    for (const auto& p : plugins_) {
        if (p->supports(path)) return p.get();
    }
    return nullptr;
}

Lang PluginRegistry::detect_lang(const fs::path& path) const {
    const auto* p = find(path);
    if (!p) return Lang::Unknown;

    const std::string id = p->id();
    if (id == "cpp") return Lang::Cpp;
    if (id == "python") return Lang::Python;
    if (id == "javascript") {
        // Distinguish TS by extension
        std::string ext = path.extension().string();
        std::string name = path.filename().string();
        // simple check
        if (ext == ".ts" || ext == ".tsx" || ext == ".mts" || ext == ".cts" ||
            name.size() >= 5 && name.compare(name.size() - 5, 5, ".d.ts") == 0)
            return Lang::TS;
        return Lang::JS;
    }
    return Lang::Unknown;
}

bool PluginRegistry::is_supported(const fs::path& path) const {
    return find(path) != nullptr;
}

bool PluginRegistry::is_interface_file(const fs::path& path) const {
    const auto* p = find(path);
    return p && p->is_interface_file(path);
}
