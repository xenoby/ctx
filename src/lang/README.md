# Language plugins for ctx

Плагины для интеграции в `collect_ctx` / `ctx`.

## Файлы

| Плагин | Расширения | Парсит |
|--------|------------|--------|
| `rust_plugin` | `.rs` | `use`, `mod` |
| `go_plugin` | `.go` | `import` / `import (...)` |
| `csharp_plugin` | `.cs` | `using` / `global using` |
| `java_plugin` | `.java` | `import` / `import static` |
| `kotlin_plugin` | `.kt`, `.kts` | `import` / `import … as` |
| `swift_plugin` | `.swift` | `import` |

## Интеграция

1. Скопировать `*_plugin.hpp` / `*_plugin.cpp` в `lang/`.
2. Includes вида `#include "../core/utils.hpp"` и `#include "language_plugin.hpp"` — как у существующих плагинов.
3. В `lang/plugin_registry.cpp`:

```cpp
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
    // new:
    plugins_.push_back(std::make_unique<RustPlugin>());
    plugins_.push_back(std::make_unique<GoPlugin>());
    plugins_.push_back(std::make_unique<CSharpPlugin>());
    plugins_.push_back(std::make_unique<JavaPlugin>());
    plugins_.push_back(std::make_unique<KotlinPlugin>());
    plugins_.push_back(std::make_unique<SwiftPlugin>());
}
```

4. В `plugin_registry.cpp` → `detect_lang()` добавить маппинг id → `Lang` **или** расширить `enum class Lang` в `core/types.hpp`:

```cpp
enum class Lang {
    Cpp, Python, JS, TS,
    Rust, Go, CSharp, Java, Kotlin, Swift,
    Unknown
};
```

и `lang_to_string()`.

5. Makefile — добавить исходники:

```
lang/rust_plugin.cpp lang/go_plugin.cpp lang/csharp_plugin.cpp \
lang/java_plugin.cpp lang/kotlin_plugin.cpp lang/swift_plugin.cpp
```

## Ограничения

- Разрешение путей — эвристика (namespace → папки), не полноценный module resolver.
- Stdlib / nuget / maven / crates.io не резолвятся (и не должны — только файлы проекта).
- Уровень 1: detection + imports. Символы/вызовы не парсятся.
