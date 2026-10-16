# ctx

Утилита сбора контекста исходников для LLM (C++ / Python / JavaScript / TypeScript).  
CLI в стиле Git + content-addressable blobs. Вывод XML или JSON.

## Сборка

```bash
make
# → ./ctx
```

## Быстрый старт

```bash
ctx init
ctx record -m "Initial snapshot"
ctx context -o task.xml          # XML
ctx context -o task.json         # JSON (по расширению)
ctx context --format json -o out # явно
```

## Команды

| Команда | Описание |
|---------|----------|
| `ctx init [path]` | Создать `.ctx/` |
| `ctx scan [path]` | Изменения с последней ревизии |
| `ctx record -m "msg"` | Зафиксировать состояние + blobs |
| `ctx history [-n N]` | История ревизий |
| `ctx show <rev> [path]` | Содержимое файла / список файлов ревизии |
| `ctx diff <revA> <revB> [path]` | Сравнить две ревизии |
| `ctx context [opts]` | Собрать контекст для LLM (XML/JSON) |
| `ctx deps <file>` | Транзитивные зависимости по импортам |

### Опции `context`

```
-o, --output <file>   выходной файл
--format xml|json     формат (по умолчанию: по расширению .json → json, иначе xml)
--entry <file>        только зависимости файла
--headers-only        только интерфейсы (.h, .pyi, .d.ts)
--full                полный снимок
```

## Workflow

```bash
ctx init
ctx record -m "Initial"

# правки...
ctx scan
ctx record -m "Fix resource reload"

ctx show r1 path/to/file.py
ctx diff r1 r2
ctx context -o task.json
```

## Хранилище `.ctx/`

```
.ctx/
├── revision
├── manifest
├── history.jsonl
├── revs/N.manifest
└── objects/ab/hash    # blobs
```

## Структура

```
main.cpp
core/          # scan, hash, revision, delta, blobs
lang/          # ILanguagePlugin
serialize/     # XmlSerializer, JsonSerializer
third_party/   # tinyxml2, nlohmann/json
```
