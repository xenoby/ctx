// ctx — Git-style CLI for collecting project context for LLMs
//
// Commands:
//   ctx init [path]
//   ctx scan [path]
//   ctx record -m "message" [path]
//   ctx history [path]
//   ctx context [path] [-o file] [--entry ...] [--headers-only] [--full]
//   ctx deps <file> [path]
//
// Build:
//   make
//   # or see Makefile

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <locale.h>
#include <sstream>
#include <memory>

#include "core/ignore.hpp"
#include "core/scanner.hpp"
#include "core/revision.hpp"
#include "core/delta.hpp"
#include "core/blob_store.hpp"
#include "core/utils.hpp"
#include "lang/plugin_registry.hpp"
#include "serialize/xml_serializer.hpp"
#include "serialize/json_serializer.hpp"
#include "third_party/nlohmann/json.hpp"

#ifdef _WIN32
#define NOMINMAX 
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================
// Helpers
// ============================================================

static fs::path find_root(const std::string& hint) {
    fs::path p = fs::absolute(hint.empty() ? "." : hint);
    // Walk up looking for .ctx
    fs::path cur = p;
    for (int i = 0; i < 20; ++i) {
        if (fs::is_directory(cur / ".ctx")) return cur;
        if (!cur.has_parent_path() || cur == cur.parent_path()) break;
        cur = cur.parent_path();
    }
    // Fallback: use given path (or cwd)
    return p;
}

static void require_init(const fs::path& root) {
    if (!is_initialized(root)) {
        std::cerr << "Ошибка: проект не инициализирован. Выполните: ctx init\n";
        std::exit(1);
    }
}

struct ScanBundle {
    PluginRegistry registry;
    std::vector<IgnorePattern> ignore;
    ScanResult scan;
    std::vector<FileInfo*> candidates;
    DeltaResult delta;
    std::unordered_map<std::string, std::string> prev_manifest;
    bool have_prev = false;
};

static ScanBundle prepare_scan(const fs::path& root, bool headers_only = false) {
    ScanBundle b;
    fs::path ignore_path = root / ".ctxignore";
    b.ignore = load_ignore_file(ignore_path);
    b.scan = scan_project(root, b.registry, b.ignore);

    for (auto& fi : b.scan.files) {
        if (headers_only && !b.registry.is_interface_file(fi.abs)) continue;
        b.candidates.push_back(&fi);
    }

    fs::path man_path = ctx_dir(root) / "manifest";
    b.prev_manifest = load_manifest(man_path);
    b.have_prev = !b.prev_manifest.empty();
    b.delta = compute_delta(b.candidates, b.scan.files, b.prev_manifest, b.have_prev);
    return b;
}

static void print_delta_summary(const DeltaResult& d) {
    if (!d.have_prev) {
        std::cout << "Нет предыдущего снимка (первый scan).\n";
        std::cout << "Файлов: " << d.added.size() << "\n";
        return;
    }
    std::cout << "Изменения:\n";
    if (!d.added.empty()) {
        std::cout << "  added (" << d.added.size() << "):\n";
        for (const auto& p : d.added) std::cout << "    + " << p << "\n";
    }
    if (!d.modified.empty()) {
        std::cout << "  modified (" << d.modified.size() << "):\n";
        for (const auto& p : d.modified) std::cout << "    ~ " << p << "\n";
    }
    if (!d.deleted.empty()) {
        std::cout << "  deleted (" << d.deleted.size() << "):\n";
        for (const auto& p : d.deleted) std::cout << "    - " << p << "\n";
    }
    if (d.added.empty() && d.modified.empty() && d.deleted.empty())
        std::cout << "  (нет изменений)\n";
}

// ============================================================
// Commands
// ============================================================

static int cmd_init(const std::vector<std::string>& args) {
    std::string path = ".";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--help") {
            std::cout << "Использование: ctx init [path]\n"
                         "Создаёт хранилище .ctx/ в указанной директории.\n";
            return 0;
        }
        if (args[i][0] != '-') path = args[i];
    }

    fs::path root = fs::absolute(path);
    if (!fs::is_directory(root)) {
        std::cerr << "Ошибка: '" << path << "' — не директория\n";
        return 1;
    }

    if (is_initialized(root)) {
        std::cout << "Уже инициализировано: " << (root / ".ctx") << "\n";
        auto rev = read_revision(root);
        std::cout << "Текущая ревизия: " << rev.current << "\n";
        return 0;
    }

    fs::create_directories(ctx_dir(root));
    write_revision(root, 0);
    // empty manifest
    save_manifest(ctx_dir(root) / "manifest", {});
    std::cout << "Инициализировано: " << (root / ".ctx") << "\n";
    return 0;
}

static int cmd_scan(const std::vector<std::string>& args) {
    std::string path = ".";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--help") {
            std::cout << "Использование: ctx scan [path]\n"
                         "Показывает изменения относительно последней зафиксированной ревизии.\n";
            return 0;
        }
        if (args[i][0] != '-') path = args[i];
    }

    fs::path root = find_root(path);
    require_init(root);

    auto b = prepare_scan(root);
    if (b.scan.files.empty()) {
        std::cout << "Не найдено подходящих файлов.\n";
        return 0;
    }

    auto rev = read_revision(root);
    std::cout << "Корень: " << root << "\n";
    std::cout << "Ревизия: " << rev.current << "\n";
    std::cout << "Файлов в проекте: " << b.scan.files.size() << "\n";
    print_delta_summary(b.delta);
    return 0;
}

static int cmd_record(const std::vector<std::string>& args) {
    std::string path = ".";
    std::string message;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--help") {
            std::cout << "Использование: ctx record -m \"message\" [path]\n"
                         "Фиксирует текущее состояние как новую ревизию.\n";
            return 0;
        }
        if ((args[i] == "-m" || args[i] == "--message") && i + 1 < args.size()) {
            message = args[++i];
        } else if (args[i][0] != '-') {
            path = args[i];
        }
    }

    if (message.empty()) {
        std::cerr << "Ошибка: укажите сообщение через -m \"...\"\n";
        return 1;
    }

    fs::path root = find_root(path);
    require_init(root);

    auto b = prepare_scan(root);
    bool has_changes = !b.delta.added.empty() || !b.delta.modified.empty() || !b.delta.deleted.empty();

    auto rev_info = read_revision(root);
    int parent = rev_info.current;
    int current = parent;

    if (!has_changes && parent > 0) {
        std::cout << "Нет изменений. Ревизия остаётся " << parent << "\n";
        return 0;
    }

    current = parent + 1;

    BlobStore blobs(root);
    blobs.store_changed(b.scan.files, b.delta.added, b.delta.modified);

    write_revision(root, current);
    save_manifest(ctx_dir(root) / "manifest", b.scan.files);
    save_rev_manifest(root, current, b.scan.files);
    append_history(root, current, parent,
                   b.delta.modified, b.delta.added, b.delta.deleted, message);

    std::cout << "Зафиксировано: r" << current;
    if (parent > 0) std::cout << " (parent r" << parent << ")";
    std::cout << "\n";
    std::cout << "Сообщение: " << message << "\n";
    print_delta_summary(b.delta);
    return 0;
}

static int cmd_history(const std::vector<std::string>& args) {
    std::string path = ".";
    int limit = 20;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--help") {
            std::cout << "Использование: ctx history [path] [-n N]\n"
                         "Показывает историю ревизий.\n";
            return 0;
        }
        if ((args[i] == "-n" || args[i] == "--max") && i + 1 < args.size()) {
            limit = std::stoi(args[++i]);
        } else if (args[i][0] != '-') {
            path = args[i];
        }
    }

    fs::path root = find_root(path);
    require_init(root);

    auto lines = read_history_lines(root);
    if (lines.empty()) {
        std::cout << "История пуста.\n";
        return 0;
    }

    int start = std::max(0, static_cast<int>(lines.size()) - limit);
    for (int i = static_cast<int>(lines.size()) - 1; i >= start; --i) {
        // Simple pretty-print: extract rev and reason if present
        const std::string& line = lines[i];
        std::cout << line << "\n";
    }
    return 0;
}


static int cmd_context(const std::vector<std::string>& args) {
    std::string path = ".";
    std::string output = "ctx.xml";
    std::string format;
    std::string entry_str;
    bool headers_only = false;
    bool full_mode = false;
    int rev_opt = -1;  // -1 = working tree

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--help") {
            std::cout <<
                "Использование: ctx context [path] [опции]\n\n"
                "Опции:\n"
                "  -o, --output <файл>  выходной файл\n"
                "  --format xml|json    формат (по расширению или xml)\n"
                "  --rev <N>            снимок ревизии N из blobs (не с диска)\n"
                "  --entry <файл>       только зависимости файла\n"
                "  --headers-only       только интерфейсные файлы\n"
                "  --full               полный снимок\n";
            return 0;
        }
        else if ((args[i] == "-o" || args[i] == "--output") && i + 1 < args.size())
            output = args[++i];
        else if (args[i] == "--entry" && i + 1 < args.size())
            entry_str = args[++i];
        else if (args[i] == "--format" && i + 1 < args.size())
            format = args[++i];
        else if (args[i] == "--rev" && i + 1 < args.size()) {
            std::string r = args[++i];
            if (!r.empty() && (r[0] == 'r' || r[0] == 'R')) r = r.substr(1);
            rev_opt = std::stoi(r);
        }
        else if (args[i] == "--headers-only") headers_only = true;
        else if (args[i] == "--full") full_mode = true;
        else if (args[i][0] != '-') path = args[i];
        else {
            std::cerr << "Неизвестный аргумент: " << args[i] << "\n";
            return 1;
        }
    }

    fs::path root = find_root(path);
    bool inited = is_initialized(root);
    PluginRegistry registry;
    auto ignore = load_ignore_file(root / ".ctxignore");

    // --- Mode: from revision blobs ---
    if (rev_opt >= 0) {
        if (!inited) {
            std::cerr << "Нужен ctx init и record с blobs\n";
            return 1;
        }
        if (!rev_manifest_exists(root, rev_opt)) {
            std::cerr << "Ревизия r" << rev_opt << " не найдена\n";
            return 1;
        }

        auto man = load_rev_manifest(root, rev_opt);
        BlobStore blobs(root);

        // Build synthetic FileInfo list from manifest
        std::vector<FileInfo> files;
        std::vector<FileInfo*> candidates;
        std::unordered_map<std::string, int> lang_counts;
        uintmax_t total_size = 0;

        for (const auto& kv : man) {
            FileInfo fi;
            fi.rel = kv.first;
            fi.hash = kv.second;
            fi.abs = root / kv.first;
            fi.lang = registry.detect_lang(fi.abs);
            if (fi.lang == Lang::Unknown) {
                // try by extension from rel path
                fi.lang = registry.detect_lang(fs::path(kv.first));
            }
            lang_counts[lang_to_string(fi.lang)]++;
            files.push_back(fi);
        }
        std::sort(files.begin(), files.end(),
                  [](const FileInfo& a, const FileInfo& b) { return a.rel < b.rel; });
        for (auto& fi : files) {
            if (headers_only && !registry.is_interface_file(fs::path(fi.rel))) continue;
            candidates.push_back(&fi);
        }

        // entry filter: only paths that appear in dep set by name match
        if (!entry_str.empty()) {
            fs::path entry_path = entry_str;
            if (!fs::exists(entry_path)) entry_path = root / entry_str;
            if (fs::exists(entry_path)) {
                auto deps = collect_deps(entry_path, root, registry);
                std::unordered_set<std::string> dep_rels;
                for (const auto& abs : deps) {
                    std::error_code ec;
                    auto r = fs::relative(abs, root, ec);
                    dep_rels.insert(ec ? abs : r.generic_string());
                }
                std::vector<FileInfo*> filtered;
                for (auto* fi : candidates)
                    if (dep_rels.count(fi->rel)) filtered.push_back(fi);
                candidates = filtered;
            }
        }

        ContextData ctx;
        ctx.mode = "full";
        ctx.revision = rev_opt;
        ctx.parent_revision = rev_opt;
        ctx.base_revision = rev_opt;
        ctx.root = root.generic_string();
        ctx.total_files = files.size();
        ctx.included_files = candidates.size();
        ctx.hash_algorithm = "fnv1a-64";
        ctx.entry = entry_str;
        ctx.headers_only = headers_only;
        ctx.languages = lang_counts;
        ctx.all_files = files;

        for (auto* fi : candidates) {
            OutputFile of;
            of.info = fi;
            of.status = "full";
            of.content_override = blobs.load(fi->hash);
            if (of.content_override.empty() && !blobs.exists(fi->hash))
                of.content_override = "[blob missing]";
            total_size += of.content_override.size();
            ctx.files_to_write.push_back(std::move(of));
            ctx.added.push_back(fi->rel);
        }
        ctx.total_size_kb = total_size / 1024;

        if (format.empty())
            format = (output.size() >= 5 && output.compare(output.size() - 5, 5, ".json") == 0) ? "json" : "xml";
        std::unique_ptr<ISerializer> serializer =
            (format == "json")
                ? std::unique_ptr<ISerializer>(std::make_unique<JsonSerializer>())
                : std::unique_ptr<ISerializer>(std::make_unique<XmlSerializer>());
        if (!serializer->write(ctx, output)) {
            std::cerr << "Ошибка записи " << output << "\n";
            return 1;
        }
        std::cout << "Режим: full (ревизия r" << rev_opt << " из blobs)\n";
        std::cout << "Файлов: " << ctx.included_files << "\n";
        std::cout << "Записано в: " << output << "\n";
        return 0;
    }

    // --- Normal mode: working tree ---
    ScanResult scan = scan_project(root, registry, ignore);
    if (scan.files.empty()) {
        std::cout << "Не найдено подходящих файлов.\n";
        return 0;
    }

    std::unordered_set<std::string> deps_set;
    bool use_entry = !entry_str.empty();
    if (use_entry) {
        fs::path entry_path = entry_str;
        if (!fs::exists(entry_path)) entry_path = root / entry_str;
        if (!fs::exists(entry_path)) {
            std::cerr << "Ошибка: --entry файл не найден: " << entry_str << "\n";
            return 1;
        }
        deps_set = collect_deps(entry_path, root, registry);
    }

    std::vector<FileInfo*> candidates;
    for (auto& fi : scan.files) {
        if (use_entry) {
            std::error_code ec;
            fs::path canon = fs::weakly_canonical(fi.abs, ec);
            if (ec) canon = fi.abs;
            if (!deps_set.count(canon.string())) continue;
        }
        if (headers_only && !registry.is_interface_file(fi.abs)) continue;
        candidates.push_back(&fi);
    }

    std::unordered_map<std::string, std::string> prev_manifest;
    bool have_prev = false;
    if (!full_mode) {
        if (inited) {
            prev_manifest = load_manifest(ctx_dir(root) / "manifest");
            have_prev = !prev_manifest.empty();
        }
        if (!have_prev && fs::exists(output)) {
            prev_manifest = parse_manifest_xml(output);
            have_prev = !prev_manifest.empty();
        }
    }

    DeltaResult delta = compute_delta(candidates, scan.files, prev_manifest, have_prev);

    int parent_rev = 0, current_rev = 0;
    if (inited) {
        auto ri = read_revision(root);
        parent_rev = ri.current;
        current_rev = parent_rev;
    }

    uintmax_t total_size = 0;
    for (const auto& fi : scan.files) total_size += fi.size;

    ContextData ctx;
    ctx.mode = have_prev ? "delta" : "full";
    ctx.revision = current_rev;
    ctx.parent_revision = parent_rev;
    ctx.base_revision = parent_rev;
    ctx.root = root.generic_string();
    ctx.total_files = scan.files.size();
    ctx.included_files = delta.files_to_write.size();
    ctx.deleted_files = delta.deleted.size();
    ctx.total_size_kb = total_size / 1024;
    ctx.entry = entry_str;
    ctx.headers_only = headers_only;
    ctx.languages = scan.lang_counts;
    ctx.all_files = scan.files;
    ctx.files_to_write = delta.files_to_write;
    ctx.deleted = delta.deleted;
    ctx.added = delta.added;
    ctx.modified = delta.modified;

    if (format.empty())
        format = (output.size() >= 5 && output.compare(output.size() - 5, 5, ".json") == 0) ? "json" : "xml";
    if (format != "xml" && format != "json") {
        std::cerr << "Неизвестный формат: " << format << "\n";
        return 1;
    }

    std::unique_ptr<ISerializer> serializer =
        (format == "json")
            ? std::unique_ptr<ISerializer>(std::make_unique<JsonSerializer>())
            : std::unique_ptr<ISerializer>(std::make_unique<XmlSerializer>());

    if (!serializer->write(ctx, output)) {
        std::cerr << "Ошибка записи " << output << "\n";
        return 1;
    }

    std::cout << "Режим: " << ctx.mode << "\n";
    if (inited) std::cout << "Ревизия: r" << current_rev << "\n";
    std::cout << "Всего файлов: " << ctx.total_files
              << ", в контексте: " << ctx.included_files;
    if (have_prev) std::cout << ", удалено: " << ctx.deleted_files;
    std::cout << "\nЗаписано в: " << output << "\n";
    return 0;
}


static int cmd_apply(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        std::cout <<
            "Использование: ctx apply <proposal.json> [path]\n\n"
            "Применяет ChangeProposal от LLM локально (сеть не нужна).\n"
            "Операции: replace_file | create_file | delete_file\n"
            "Проверяет base_revision и expected_old_hash.\n";
        return args.empty() ? 1 : 0;
    }

    std::string proposal_path = args[0];
    std::string root_hint = args.size() > 1 ? args[1] : ".";
    fs::path root = find_root(root_hint);
    require_init(root);

    std::string raw;
    if (!read_file_content(proposal_path, raw)) {
        std::cerr << "Не удалось прочитать " << proposal_path << "\n";
        return 1;
    }

    nlohmann::json prop;
    try {
        prop = nlohmann::json::parse(raw);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка JSON: " << e.what() << "\n";
        return 1;
    }

    int base_rev = prop.value("base_revision", -1);
    auto ri = read_revision(root);
    if (base_rev >= 0 && base_rev != ri.current) {
        std::cerr << "Отказ: proposal от r" << base_rev
                  << ", текущая ревизия r" << ri.current << "\n"
                  << "Сделайте ctx record или поправьте base_revision.\n";
        return 1;
    }

    if (!prop.contains("changes") || !prop["changes"].is_array()) {
        std::cerr << "Нет массива changes\n";
        return 1;
    }

    int ok = 0, fail = 0;
    for (const auto& ch : prop["changes"]) {
        std::string op = ch.value("operation", "replace_file");
        std::string rel = ch.value("path", "");
        if (rel.empty()) {
            std::cerr << "  skip: пустой path\n";
            ++fail;
            continue;
        }
        // safety: no path escape
        if (rel.find("..") != std::string::npos || !rel.empty() && rel[0] == '/') {
            std::cerr << "  skip unsafe path: " << rel << "\n";
            ++fail;
            continue;
        }
        fs::path dest = root / rel;

        if (op == "delete_file") {
            std::error_code ec;
            if (fs::remove(dest, ec)) {
                std::cout << "  deleted " << rel << "\n";
                ++ok;
            } else {
                std::cerr << "  fail delete " << rel << "\n";
                ++fail;
            }
            continue;
        }

        std::string content = ch.value("content", "");
        std::string expected = ch.value("expected_old_hash", "");

        if (op == "replace_file" || op == "create_file") {
            if (op == "replace_file" && !expected.empty() && fs::exists(dest)) {
                auto hr = hash_file(dest);
                std::string cur = fnv1a_hex(hr.hash);
                if (cur != expected) {
                    std::cerr << "  conflict " << rel
                              << ": expected " << expected
                              << ", got " << cur << "\n";
                    ++fail;
                    continue;
                }
            }
            fs::create_directories(dest.parent_path());
            std::ofstream out(dest, std::ios::binary);
            if (!out) {
                std::cerr << "  fail write " << rel << "\n";
                ++fail;
                continue;
            }
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
            std::cout << "  " << op << " " << rel << "\n";
            ++ok;
        } else {
            std::cerr << "  unknown operation: " << op << "\n";
            ++fail;
        }
    }

    std::cout << "Применено: " << ok << ", ошибок: " << fail << "\n";
    if (prop.contains("rationale") && prop["rationale"].is_array()) {
        std::cout << "Rationale:\n";
        for (const auto& r : prop["rationale"])
            std::cout << "  - " << r.get<std::string>() << "\n";
    }
    std::cout << "Совет: проверьте diff и выполните ctx record -m \"...\"\n";
    return fail ? 1 : 0;
}


static int cmd_deps(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        std::cout << "Использование: ctx deps <file> [path]\n"
                     "Показывает транзитивные зависимости файла по импортам.\n";
        return args.empty() ? 1 : 0;
    }

    std::string file = args[0];
    std::string path = ".";
    if (args.size() > 1 && args[1][0] != '-') path = args[1];

    fs::path root = find_root(path);
    PluginRegistry registry;

    fs::path entry_path = file;
    if (!fs::exists(entry_path)) entry_path = root / file;
    if (!fs::exists(entry_path)) {
        std::cerr << "Файл не найден: " << file << "\n";
        return 1;
    }

    auto deps = collect_deps(entry_path, root, registry);
    std::cout << "Зависимости " << file << " (" << deps.size() << "):\n";

    // Print relative paths sorted
    std::vector<std::string> rels;
    for (const auto& abs : deps) {
        std::error_code ec;
        fs::path r = fs::relative(abs, root, ec);
        rels.push_back(ec ? abs : r.generic_string());
    }
    std::sort(rels.begin(), rels.end());
    for (const auto& r : rels)
        std::cout << "  " << r << "\n";
    return 0;
}

// ============================================================


static int parse_rev(const std::string& s) {
    std::string t = s;
    if (!t.empty() && (t[0] == 'r' || t[0] == 'R')) t = t.substr(1);
    return std::stoi(t);
}

static int cmd_show(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        std::cout << "Использование: ctx show <rev> [path]\n"
                     "Показать содержимое файла в указанной ревизии.\n"
                     "Без path — список файлов ревизии.\n";
        return args.empty() ? 1 : 0;
    }

    int rev = 0;
    try { rev = parse_rev(args[0]); }
    catch (...) {
        std::cerr << "Неверный номер ревизии: " << args[0] << "\n";
        return 1;
    }

    std::string file_path;
    std::string root_hint = ".";
    if (args.size() >= 2) file_path = args[1];
    if (args.size() >= 3) root_hint = args[2];

    fs::path root = find_root(root_hint);
    require_init(root);

    if (!rev_manifest_exists(root, rev)) {
        std::cerr << "Ревизия r" << rev << " не найдена (нет манифеста).\n";
        return 1;
    }

    auto man = load_rev_manifest(root, rev);

    if (file_path.empty()) {
        std::cout << "Файлы в r" << rev << " (" << man.size() << "):\n";
        std::vector<std::string> paths;
        for (const auto& kv : man) paths.push_back(kv.first);
        std::sort(paths.begin(), paths.end());
        for (const auto& p : paths)
            std::cout << "  " << p << "  " << man[p] << "\n";
        return 0;
    }

    auto it = man.find(file_path);
    if (it == man.end()) {
        std::cerr << "Файл '" << file_path << "' отсутствует в r" << rev << "\n";
        return 1;
    }

    BlobStore blobs(root);
    std::string content = blobs.load(it->second);
    if (content.empty() && !blobs.exists(it->second)) {
        std::cerr << "Блоб " << it->second << " не найден.\n";
        return 1;
    }
    std::cout << content;
    if (!content.empty() && content.back() != '\n') std::cout << "\n";
    return 0;
}

static int cmd_diff(const std::vector<std::string>& args) {
    if (args.size() < 2 || args[0] == "-h" || args[0] == "--help") {
        std::cout << "Использование: ctx diff <revA> <revB> [path]\n"
                     "Сравнить две ревизии. С path — построчное сравнение файла.\n";
        return args.size() < 2 ? 1 : 0;
    }

    int rev_a = 0, rev_b = 0;
    try {
        rev_a = parse_rev(args[0]);
        rev_b = parse_rev(args[1]);
    } catch (...) {
        std::cerr << "Неверные номера ревизий\n";
        return 1;
    }

    std::string only_path;
    std::string root_hint = ".";
    if (args.size() >= 3 && args[2][0] != '-') only_path = args[2];
    if (args.size() >= 4) root_hint = args[3];

    fs::path root = find_root(root_hint);
    require_init(root);

    if (!rev_manifest_exists(root, rev_a) || !rev_manifest_exists(root, rev_b)) {
        std::cerr << "Одна из ревизий не найдена.\n";
        return 1;
    }

    auto man_a = load_rev_manifest(root, rev_a);
    auto man_b = load_rev_manifest(root, rev_b);

    std::unordered_set<std::string> all;
    for (const auto& kv : man_a) all.insert(kv.first);
    for (const auto& kv : man_b) all.insert(kv.first);

    std::vector<std::string> added, removed, changed;
    for (const auto& p : all) {
        if (!only_path.empty() && p != only_path) continue;
        bool in_a = man_a.count(p) > 0;
        bool in_b = man_b.count(p) > 0;
        if (!in_a && in_b) added.push_back(p);
        else if (in_a && !in_b) removed.push_back(p);
        else if (in_a && in_b && man_a[p] != man_b[p]) changed.push_back(p);
    }
    std::sort(added.begin(), added.end());
    std::sort(removed.begin(), removed.end());
    std::sort(changed.begin(), changed.end());

    std::cout << "diff r" << rev_a << " -> r" << rev_b << "\n";
    for (const auto& p : added)   std::cout << "  + " << p << "\n";
    for (const auto& p : removed) std::cout << "  - " << p << "\n";
    for (const auto& p : changed) std::cout << "  ~ " << p << "\n";

    if (added.empty() && removed.empty() && changed.empty())
        std::cout << "  (нет различий)\n";

    if (!only_path.empty() && std::find(changed.begin(), changed.end(), only_path) != changed.end()) {
        BlobStore blobs(root);
        std::string ca = blobs.load(man_a[only_path]);
        std::string cb = blobs.load(man_b[only_path]);
        std::cout << "\n--- r" << rev_a << ":" << only_path << "\n";
        std::cout << "+++ r" << rev_b << ":" << only_path << "\n";

        auto to_lines = [](const std::string& s) {
            std::vector<std::string> out;
            std::istringstream ss(s);
            std::string line;
            while (std::getline(ss, line)) out.push_back(line);
            return out;
        };
        auto la = to_lines(ca), lb = to_lines(cb);
        size_t max_show = 200;
        size_t i = 0, j = 0, shown = 0;
        while ((i < la.size() || j < lb.size()) && shown < max_show) {
            if (i < la.size() && j < lb.size() && la[i] == lb[j]) {
                ++i; ++j;
                continue;
            }
            if (i < la.size()) { std::cout << "- " << la[i++] << "\n"; ++shown; }
            if (j < lb.size() && shown < max_show) { std::cout << "+ " << lb[j++] << "\n"; ++shown; }
        }
        if (i < la.size() || j < lb.size())
            std::cout << "... (обрезано)\n";
    }

    return 0;
}

// Help & main
// ============================================================

static void print_help() {
    std::cout <<
        "ctx — сбор контекста проекта для LLM (C++ / Python / JS / TS/ Rust/ Go/ CSharp/ Java/ Kotlin/ Swift)\n\n"
        "Использование:\n"
        "  ctx <команда> [аргументы]\n\n"
        "Команды:\n"
        "  init [path]              Инициализировать .ctx/ в проекте\n"
        "  scan [path]              Показать изменения с последней ревизии\n"
        "  record -m \"msg\" [path]   Зафиксировать состояние + blobs\n"
        "  history [path] [-n N]    История ревизий\n"
        "  show <rev> [path]        Содержимое файла / список файлов ревизии\n"
        "  diff <revA> <revB> [path] Сравнить две ревизии\n"
        "  context [path] [opts]    Контекст XML/JSON (--rev, --format)\n"
        "  apply <proposal.json>    Применить ChangeProposal от LLM\n"
        "  deps <file> [path]       Показать зависимости файла\n"
        "  help                     Эта справка\n\n"
        "Примеры:\n"
        "  ctx init && ctx record -m \"Initial\"\n"
        "  ctx context -o task.json\n"
        "  ctx context --rev 1 -o old.json\n"
        "  ctx apply proposal.json\n"
        "  ctx diff r1 r2\n";
}

int main(int argc, char** argv) {

#ifdef _WIN32
    // Переключаем консоль Windows на UTF-8 (код 65001)
    SetConsoleOutputCP(CP_UTF8);
    // Опционально: разрешаем ввод UTF-8 (если будете читать русский текст)
    SetConsoleCP(CP_UTF8);
#else
    setlocale(LC_ALL, "Russian");
#endif

    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string cmd = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i)
        args.push_back(argv[i]);

    if (cmd == "help" || cmd == "-h" || cmd == "--help") {
        print_help();
        return 0;
    }
    if (cmd == "init")    return cmd_init(args);
    if (cmd == "scan")    return cmd_scan(args);
    if (cmd == "record")  return cmd_record(args);
    if (cmd == "history") return cmd_history(args);
    if (cmd == "show")    return cmd_show(args);
    if (cmd == "diff")    return cmd_diff(args);
    if (cmd == "context") return cmd_context(args);
    if (cmd == "apply")   return cmd_apply(args);
    if (cmd == "deps")    return cmd_deps(args);

    std::cerr << "Неизвестная команда: " << cmd << "\n"
              << "Выполните: ctx help\n";
    return 1;
}
