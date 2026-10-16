# ctx — контекст проекта для LLM

Утилита собирает согласованный снимок исходников, ведёт ревизии с content-addressable blobs и помогает безопасно применять изменения, предложенные моделью.

Поддерживаемые языки (через плагины): **C++**, **Python**, **JavaScript/TypeScript**, а также (при подключении доп. плагинов) Rust, Go, C#, Java, Kotlin, Swift.

---

## Оглавление

1. [Зачем это нужно](#1-зачем-это-нужно)
2. [Сборка](#2-сборка)
3. [Основные понятия](#3-основные-понятия)
4. [Два режима работы с моделью](#4-два-режима-работы-с-моделью)
5. [Базовый workflow (чат / удалённая модель)](#5-базовый-workflow-чат--удалённая-модель)
6. [Workflow с локальной моделью](#6-workflow-с-локальной-моделью)
7. [Справочник команд](#7-справочник-команд)
8. [Формат ChangeProposal](#8-формат-changeproposal)
9. [Формат контекста (XML / JSON)](#9-формат-контекста-xml--json)
10. [Хранилище `.ctx/`](#10-хранилище-ctx)
11. [Типичные сценарии](#11-типичные-сценарии)
12. [Рекомендации и ограничения](#12-рекомендации-и-ограничения)

---

## 1. Зачем это нужно

При работе с LLM типичные проблемы:

| Проблема | Как ctx помогает |
|----------|------------------|
| Модель не видит весь проект | Собирает релевантные файлы (полный снимок или delta) |
| Модель думает про устаревший код | Ревизии + `base_revision` в proposal |
| Непонятно, что изменилось | `scan`, `diff`, блок changes |
| Патч «вслепую» ломает файлы | `apply` с проверкой хэша |
| Сессия чата оборвалась | `context --rev N` восстанавливает снимок из blobs |
| Слишком много токенов | `--entry`, `--headers-only`, delta-режим |

---

## 2. Сборка

```bash
cd collect_ctx
make
# бинарник: ./ctx
```

Или вручную:

```bash
g++ -std=c++17 -O2 \
  main.cpp \
  core/ignore.cpp core/revision.cpp core/scanner.cpp core/delta.cpp core/blob_store.cpp \
  lang/plugin_registry.cpp lang/cpp_plugin.cpp lang/python_plugin.cpp lang/js_plugin.cpp \
  serialize/xml_serializer.cpp serialize/json_serializer.cpp \
  third_party/tinyxml2/tinyxml2.cpp \
  -o ctx
```

---

## 3. Основные понятия

### Ревизия

Зафиксированное состояние проекта после `ctx record`. Номер хранится в `.ctx/revision`.

### Manifest

Таблица `path → hash` всех исходников. Есть:

- `.ctx/manifest` — **текущее** зафиксированное дерево
- `.ctx/revs/N.manifest` — дерево **конкретной** ревизии N

### Blob

Содержимое файла, адресуемое по хэшу (FNV-1a 64-bit). Лежит в `.ctx/objects/ab/hash…`.
Неизменившиеся файлы между ревизиями **шарят один blob**.

### Delta

В контекст попадают только **новые и изменённые** файлы относительно последнего манифеста. Полный список путей и хэшей всё равно есть в манифесте.

### ChangeProposal

JSON-файл с предложенными изменениями от модели: какие файлы заменить/создать/удалить, от какой ревизии, с каким ожидаемым старым хэшем.

---

## 4. Два режима работы с моделью

Важно различать, **как** модель связана с вашим диском.

### A. Чат / удалённый API

(Grok, ChatGPT, Claude в браузере, API без доступа к файловой системе)

Модель **не может** писать файлы на ваш компьютер. Она только генерирует текст.

```
вы  ── ctx context ──►  файл контекста  ──►  модель
модель  ──►  текст / JSON proposal  ──►  вы сохраняете proposal.json
вы  ── ctx apply proposal.json ──►  диск
вы  ── ctx record -m "…" ──►  новая ревизия
```

Здесь `apply` — контролируемый способ перенести ответ модели на диск с проверками (`base_revision`, `expected_old_hash`).

### B. Локальная модель с доступом к файлам

(Cursor agent, Continue, Aider, свой агент с tool-calling, LLM + MCP filesystem и т.п.)

Модель **может** сама читать и писать файлы. Тогда:

- `ctx` полезен как **источник истины** (ревизии, diff, история);
- `apply` не обязателен — агент пишет напрямую;
- после правок агента стоит делать `ctx record`, чтобы зафиксировать состояние.

```
агент читает/правит файлы сам
        │
        ▼
ctx scan          # что изменилось?
ctx record -m "…" # зафиксировать
ctx context       # согласованный снимок для другой сессии / модели
```

**Итог:** фраза «модель не пишет на диск» относится к **чату и удалённому API**.  
Локальный агент с tools пишет сам; `ctx` в этом случае про историю и согласованность, а не про единственный канал записи.

---

## 5. Базовый workflow (чат / удалённая модель)

### Первый раз

```bash
cd /path/to/your/project
ctx init
ctx record -m "Initial snapshot"
ctx context --full -o task.json
```

Отправьте `task.json` (или `.xml`) в чат вместе с задачей, например:

> Вот контекст проекта (ревизия в summary).  
> Нужно: …  
> Верни изменения строго в формате ChangeProposal JSON.

## Дальше по циклу
# правки в коде...
ctx scan
ctx record -m "Fix something"
ctx context -o task.json           # только delta — нормально

### Итерация

```bash
# 1. Модель прислала proposal — сохранили в proposal.json
ctx apply proposal.json

# 2. Проверили глазами / тестами
# 3. Зафиксировали
ctx record -m "Fix resource reload"

# 4. Новый контекст для следующей итерации (обычно delta)
ctx context -o task.json
```

### Если модель «забыла» сессию

```bash
ctx context --rev 5 -o resume.json
```

Полный снимок **как в ревизии 5**, из blobs — даже если рабочее дерево уже ушло вперёд.

### Если proposal от старой ревизии

```text
Отказ: proposal от r3, текущая ревизия r7
```

Варианты:

1. `ctx context --rev 3 -o old.json` — дать модели снимок её ревизии и попросить proposal заново;
2. вручную поправить `base_revision` (рискованно);
3. восстановить нужные файлы из blobs через `ctx show` и повторить.

---

## 6. Workflow с локальной моделью

```bash
ctx init
ctx record -m "Start"

# … агент правит код через tools …

ctx scan
ctx record -m "Agent: add JWT auth"
ctx context -o handoff.json   # другой модели / человеку
```

| Цель | Команда |
|------|---------|
| Что изменилось с last record | `ctx scan` |
| Зафиксировать точку | `ctx record -m "…"` |
| Сравнить две точки | `ctx diff r4 r7` |
| Старая версия файла | `ctx show r4 path/to/file.py` |
| Контекст вокруг модуля | `ctx context --entry src/main.py -o m.json` |

`apply` здесь опционален: полезен, если локальная модель по промпту всё равно отдаёт proposal-файлом, а не пишет через tools.

---

## 7. Справочник команд

```text
ctx init [path]                 Создать .ctx/
ctx scan [path]                 Изменения vs последняя ревизия
ctx record -m "msg" [path]      Новая ревизия + blobs
ctx history [path] [-n N]       Журнал ревизий
ctx show <rev> [path]           Файл или список файлов ревизии
ctx diff <revA> <revB> [path]   Сравнение ревизий
ctx context [path] [opts]       Собрать контекст для LLM
ctx apply <proposal.json>       Применить proposal (локально, без сети)
ctx deps <file> [path]          Транзитивные зависимости по импортам
ctx help
```

### Опции `context`

| Опция | Смысл |
|-------|--------|
| `-o, --output FILE` | Куда писать |
| `--format xml\|json` | Формат (иначе по расширению `.json` → json) |
| `--rev N` | Снимок из blobs ревизии N, не с диска |
| `--entry FILE` | Только файл + зависимости по импортам |
| `--headers-only` | Только интерфейсы (`.h`, `.pyi`, `.d.ts`, …) |
| `--full` | Полный снимок, игнорировать delta |

---

## 8. Формат ChangeProposal

```json
{
  "base_revision": 3,
  "changes": [
    {
      "operation": "replace_file",
      "path": "world/world_loader.py",
      "expected_old_hash": "a1b2c3d4e5f60708",
      "content": "полный новый текст файла\n"
    },
    {
      "operation": "create_file",
      "path": "world/helpers.py",
      "content": "..."
    },
    {
      "operation": "delete_file",
      "path": "world/old_util.py"
    }
  ],
  "rationale": [
    "Preserve resource identity on reload",
    "Do not change public API"
  ]
}
```

| Поле | Назначение |
|------|------------|
| `base_revision` | Ревизия, от которой считала модель. `apply` откажет, если текущая другая |
| `expected_old_hash` | Хэш файла **до** правки. Защита от параллельных изменений |
| `operation` | `replace_file` \| `create_file` \| `delete_file` |
| `path` | Относительный путь (без `..` и абсолютных путей) |
| `rationale` | Зачем меняли |

`apply` **не** вызывает `record` — после проверки сделайте `ctx record -m "…"`.

Сеть для `apply` **не нужна**: proposal — обычный локальный файл.

---

## 9. Формат контекста (XML / JSON)

Одинаковая структура, разная сериализация.

- **summary** — mode (`full`/`delta`), revision, parent/base_revision, root, счётчики
- **languages** — сколько файлов какого языка
- **changes** — added / modified / deleted
- **hash_manifest** — все path+hash проекта
- **project_structure** — список путей
- **files** — содержимое только включённых в этот пакет файлов

В delta-режиме в `files` мало объектов, но манифест полный: модель видит «что ещё есть», не таща все тела файлов.

---

## 10. Хранилище `.ctx/`

```text
.ctx/
├── revision            # число текущей ревизии
├── manifest            # path<TAB>hash  (последний record)
├── history.jsonl       # по строке на ревизию + reason
├── revs/
│   ├── 1.manifest
│   └── 2.manifest
└── objects/
    └── a1/
        └── a1b2c3…     # blob содержимого
```

Обычно `.ctx/` не коммитят в git (или коммитят осознанно).  
Выходные `task.json` / `ctx.xml` лучше добавить в `.gitignore`.

Игнор исходников: `.ctxignore` в корне (+ встроенные `node_modules`, `__pycache__`, `.git`, `.ctx`, …).

---

## 11. Типичные сценарии

### Один модуль + зависимости

```bash
ctx context --entry engine/game_engine.py -o engine.json
```

### Только интерфейсы

```bash
ctx context --headers-only -o api.xml
```

### Что сделал агент с прошлого record

```bash
ctx scan
```

### Откат файла к старой ревизии

```bash
ctx show r5 world/world_loader.py > world/world_loader.py
ctx record -m "Revert world_loader to r5"
```

### Передача другой модели снимка r8

```bash
ctx context --rev 8 -o for_other_model.json
```

---

## 12. Рекомендации и ограничения

**Стоит**

- `record` после каждого осмысленного куска работы
- В `-m` писать **зачем**, не только «update»
- Для чата просить ChangeProposal с `base_revision` и хэшами из контекста
- После `apply` — тесты, потом `record`

**Не стоит ждать**

- Semantic analysis (типы, overload) — только файлы + импорты
- Резолва nuget/maven/crates.io/stdlib — только дерево проекта
- Автоmerge при конфликте хэшей — `apply` откажется намеренно

**Сеть**

- Сама утилита `ctx` в сеть **не ходит**
- Сеть нужна только клиенту LLM (браузер, API)
- `apply`, `record`, `show`, `diff` — всегда локальные

---

## Быстрая шпаргалка

```bash
# старт
ctx init
ctx record -m "Initial"

# отдать в чат
ctx context -o task.json

# принять ответ модели (чат / без доступа к диску)
ctx apply proposal.json
ctx record -m "Описание изменения"

# агент уже поправил файлы сам
ctx scan
ctx record -m "Agent: …"

# история
ctx history
ctx show r3 path/file.py
ctx diff r2 r5
ctx context --rev 3 -o old.json
```
