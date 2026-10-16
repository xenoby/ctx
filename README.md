# ctx — Project Context for LLMs

[Читать на русском языке](README.ru.md)

A utility that captures a consistent snapshot of source files, maintains revisions using content-addressable blobs, and helps safely apply model-proposed changes.

Supported languages (via plugins): **C++**, **Python**, **JavaScript/TypeScript**, as well as (by connecting additional plugins) Rust, Go, C#, Java, Kotlin, Swift.

---

## Table of Contents

1. [Why Use It](#1-why-use-it)
2. [Building](#2-building)
3. [Core Concepts](#3-core-concepts)
4. [Two Modes of Operation with a Model](#4-two-modes-of-operation-with-a-model)
5. [Basic Workflow (Chat / Remote Model)](#5-basic-workflow-chat--remote-model)
6. [Workflow with a Local Model](#6-workflow-with-a-local-model)
7. [Command Reference](#7-command-reference)
8. [ChangeProposal Format](#8-changeproposal-format)
9. [Context Format (XML / JSON)](#9-context-format-xml--json)
10. [The `.ctx/` Storage](#10-the-ctx-storage)
11. [Typical Scenarios](#11-typical-scenarios)
12. [Best Practices and Limitations](#12-best-practices-and-limitations)

---

## 1. Why Use It

Typical issues when working with LLMs:

| Problem | How ctx Helps |
|----------|------------------|
| Model cannot see the whole project | Gathers relevant files (full snapshot or delta) |
| Model reasons about outdated code | Revisions + `base_revision` checked in the proposal |
| Unclear what actually changed | Run `scan`, `diff`, or inspect the changes block |
| "Blind" patching corrupts files | Runs `apply` with mandatory hash validation |
| Chat session timed out or broke | Run `context --rev N` to rebuild the snapshot from blobs |
| Token limit exceeded | Use `--entry`, `--headers-only`, or delta mode |

---

## 2. Building

```bash
cd collect_ctx
make
# binary output: ./ctx
```

Or manually:

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

## 3. Core Concepts

### Revision

A recorded state of the project after running `ctx record`. The current revision number is kept in `.ctx/revision`.

### Manifest

A `path → hash` lookup table of all source files. It exists in two forms:

- `.ctx/manifest` — the **currently** recorded tree
- `.ctx/revs/N.manifest` — the tree for a **specific** revision N

### Blob

File contents addressed entirely by their hash (FNV-1a 64-bit). Stored under `.ctx/objects/ab/hash…`.
Files that do not change between revisions **share the same blob**.

### Delta

In delta mode, the context captures only **new and modified** files relative to the latest manifest. The complete list of paths and hashes is still present in the manifest summary.

### ChangeProposal

A JSON file containing the changes proposed by the model: which files to replace, create, or delete, relative to which base revision, and with what expected old hash.

---

## 4. Two Modes of Operation with a Model

It is essential to distinguish **how** the model is connected to your local disk.

### A. Chat / Remote API

(Grok, ChatGPT, Claude in the browser, or an API without filesystem access)

The model **cannot** write files directly to your machine. It only generates text output.

```
you  ── ctx context ──►  context file  ──►  model
model  ──►  text / JSON proposal  ──►  you save proposal.json
you  ── ctx apply proposal.json ──►  disk
you  ── ctx record -m "…" ──►  new revision
```

Here, `apply` acts as a controlled way to apply the model's response to your disk with rigorous checks (`base_revision`, `expected_old_hash`).

### B. Local Model with Filesystem Access

(Cursor agent, Continue, Aider, a custom tool-calling agent, an LLM + MCP filesystem, etc.)

The model **can** read and write files on its own. In this setup:

- `ctx` is useful as a **source of truth** (revisions, diffs, history);
- `apply` is optional since the agent modifies files directly;
- you should run `ctx record` after the agent finishes its edits to commit the state.

```
agent reads/edits files directly
        │
        ▼
ctx scan          # what changed?
ctx record -m "…" # commit state
ctx context       # consistent snapshot for another session / model
```

**Summary:** the phrase "the model does not write to disk" applies strictly to **chats and remote APIs**.  
A local agent with tools writes directly; in that case, `ctx` ensures history and consistency rather than acting as the sole write channel.

---

## 5. Basic Workflow (Chat / Remote Model)

### First-time Setup

```bash
cd /path/to/your/project
ctx init
ctx record -m "Initial snapshot"
ctx context --full -o task.json
```

Send `task.json` (or `.xml`) to the chat along with your prompt, for example:

> Here is the project context (the exact revision is in the summary).  
> Task: …  
> Return the changes strictly in the ChangeProposal JSON format.

### Regular Cycle
```bash
# make some code changes...
ctx scan
ctx record -m "Fix something"
ctx context -o task.json           # delta mode is perfectly fine here
```

### Iteration

```bash
# 1. The model provided a proposal — save it to proposal.json
ctx apply proposal.json

# 2. Verify changes manually / run tests
# 3. Commit the state
ctx record -m "Fix resource reload"

# 4. Generate a new context for the next iteration (usually a delta)
ctx context -o task.json
```

### Resuming an Interrupted Session

```bash
ctx context --rev 5 -o resume.json
```

This generates a full snapshot **exactly as it was in revision 5** from blobs, even if your working directory has moved far ahead.

### Handling an Outdated Proposal

```text
Rejected: proposal is for r3, current revision is r7
```

Your options:

1. `ctx context --rev 3 -o old.json` — provide the model with the exact snapshot of the revision it used and ask it to regenerate the proposal;
2. manually update `base_revision` in the JSON (risky);
3. restore the required files from blobs via `ctx show` and try again.

---

## 6. Workflow with a Local Model

```bash
ctx init
ctx record -m "Start"

# … agent modifies the code via tools …

ctx scan
ctx record -m "Agent: add JWT auth"
ctx context -o handoff.json   # hand off to another model or developer
```

| Goal | Command |
|------|---------|
| See changes since the last record | `ctx scan` |
| Save a checkpoint | `ctx record -m "…"` |
| Compare two checkpoints | `ctx diff r4 r7` |
| View an old version of a file | `ctx show r4 path/to/file.py` |
| Context narrowed down to a module | `ctx context --entry src/main.py -o m.json` |

`apply` is optional here: it remains useful if the local model still returns a proposal file via prompt instead of writing directly through tools.

---

## 7. Command Reference

```text
ctx init [path]                 Create .ctx/ directory
ctx scan [path]                 Show modifications vs latest revision
ctx record -m "msg" [path]      Create a new revision + store blobs
ctx history [path] [-n N]       Show revision log
ctx show <rev> [path]           View a file or file list from a revision
ctx diff <revA> <revB> [path]   Compare two revisions
ctx context [path] [opts]       Assemble context package for LLM
ctx apply <proposal.json>       Apply a proposal locally (offline)
ctx deps <file> [path]          Trace transitive import dependencies
ctx help
```

### `context` Options

| Option | Description |
|-------|--------|
| `-o, --output FILE` | Destination output file |
| `--format xml\|json` | Explicit format choice (defaults by extension, e.g., `.json` → json) |
| `--rev N` | Reconstruct snapshot from revision N blobs instead of using the disk |
| `--entry FILE` | Include only this file and its tracked import dependencies |
| `--headers-only` | Include interface definitions only (`.h`, `.pyi`, `.d.ts`, etc.) |
| `--full` | Force a full snapshot, bypassing delta mode |

---

## 8. ChangeProposal Format

```json
{
  "base_revision": 3,
  "changes": [
    {
      "operation": "replace_file",
      "path": "world/world_loader.py",
      "expected_old_hash": "a1b2c3d4e5f60708",
      "content": "full new file content here\n"
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

| Field | Purpose |
|------|------------|
| `base_revision` | The revision the model used for calculations. `apply` will fail if the current disk revision differs |
| `expected_old_hash` | Hash of the file **before** modification. Guards against concurrent edits |
| `operation` | `replace_file` \| `create_file` \| `delete_file` |
| `path` | Relative path (forbids `..` and absolute paths) |
| `rationale` | Explanation of why the changes were made |

`apply` **does not** automatically trigger `record` — run `ctx record -m "…"` manually after reviewing.

Network access is **not required** for `apply`: the proposal is just a local file.

---

## 9. Context Format (XML / JSON)

Identical data structure, different serialization flavors.

- **summary** — mode (`full`/`delta`), revision, parent/base_revision, root directory, metrics
- **languages** — count of files grouped by language
- **changes** — tracked additions / modifications / deletions
- **hash_manifest** — a complete project-wide mapping of paths to hashes
- **project_structure** — flat list of directory paths
- **files** — literal contents of the files included in this specific package

In delta mode, `files` contains very few objects, but the manifest remains complete: the model can see "what else exists" without pulling massive file bodies.

---

## 10. The `.ctx/` Storage

```text
.ctx/
├── revision            # Current revision number
├── manifest            # path<TAB>hash mapping (from the latest record)
├── history.jsonl       # Line-by-line revision logs with reason strings
├── revs/
│   ├── 1.manifest
│   └── 2.manifest
└── objects/
    └── a1/
        └── a1b2c3…     # Content-addressable blob
```

Typically, `.ctx/` is excluded from git (unless committed intentionally).  
Generated output files like `task.json` / `ctx.xml` should be added to your `.gitignore`.

Excluding source files: Create a `.ctxignore` file in the root directory (built-in defaults include `node_modules`, `__pycache__`, `.git`, `.ctx`, etc.).

---

## 11. Typical Scenarios

### Target a Single Module with Dependencies

```bash
ctx context --entry engine/game_engine.py -o engine.json
```

### Export Interfaces Only

```bash
ctx context --headers-only -o api.xml
```

### Check Agent Progress Since Last Record

```bash
ctx scan
```

### Revert a File to an Older Revision

```bash
ctx show r5 world/world_loader.py > world/world_loader.py
ctx record -m "Revert world_loader to r5"
```

### Pass a Specific Revision Checkpoint (r8) to a Different Model

```bash
ctx context --rev 8 -o for_other_model.json
```

---

## 12. Best Practices and Limitations

**Do's**

- Run `record` after completing any logical chunk of work
- Use the `-m` flag to describe **why** changes were made, not just generic "updates"
- Instruct chat models to format responses as a ChangeProposal using `base_revision` and hashes from the context
- Run your test suite after an `apply` before executing a `record`

**Don't Expect**

- Deep semantic analysis (type checking, overload resolution) — it maps files and literal imports
- Package manager resolution (nuget, maven, crates.io, stdlib) — it tracks the project directory tree only
- Auto-merging during hash conflicts — `apply` intentionally rejects conflicting files

**Network Requirements**

- The `ctx` utility operates **completely offline**
- Network connectivity is only utilized by your LLM client (browser, API layer)
- Commands like `apply`, `record`, `show`, and `diff` are strictly local

---

## Quick Cheat Sheet

```bash
# Initialize and start
ctx init
ctx record -m "Initial"

# Package context for a chat model
ctx context -o task.json

# Apply model response (chat / remote workflows)
ctx apply proposal.json
ctx record -m "Describe the applied change"

# When an agent has already edited files directly
ctx scan
ctx record -m "Agent: …"

# Navigation and history
ctx history
ctx show r3 path/file.py
ctx diff r2 r5
ctx context --rev 3 -o old.json
```
