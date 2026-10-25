@page TrustLiMe_dc_skills OpenCode skills and workflow

This page documents the **OpenCode skills** shipped with trust-LiMe and the
**standard workflow** for working in this repository. The skills live under
`.opencode/skills/<id>/SKILL.md`; load one with the `skill` tool when a task
matches its description.

## 1. Skills

### `trust-code-navigation`
Navigate the TRUST and TrioCFD source trees (`../trust-code`,
`~/Development/triocfd-code`) through prebuilt ctags indexes.
- **When to use:** finding where a class/function/macro is defined or declared
  in the kernel or TrioCFD; rebuilding indexes after a codebase update.
- **Key commands:** `.opencode/scripts/index-codebases.sh`,
  `.opencode/scripts/find-symbol.sh <name>` (or `/find-symbol`).
- **Notes:** indexes live in `.index/` (gitignored); src/ only.

### `trust-lime-coding`
Write and modify trust-LiMe C++ kernel-extension code.
- **When to use:** editing `src/*.{h,cpp}`, adding a class or keyword, fixing
  compile warnings/errors.
- **Key points:** deriving baltik classes from TRUST bases, `Param`-based
  keyword reading, association/ref patterns, strict compiler flags
  (`-Werror -Wconversion`), `// XD` keyword documentation.
- **First read:** `docs/content/dev_corner/kernel_reuse.md`.

### `trust-lime-docs`
Build, extend, and publish the documentation.
- **When to use:** adding/editing `docs/content/*.md`, running `make docs`,
  debugging the doc build, opening the site in a browser.
- **Key commands:** `make docs` (or `cd docs && make`); output in
  `docs/build/html/index.html`.

### `trust-lime-env`
Set up and use the TRUST + trust-LiMe runtime environment.
- **When to use:** any command needing the TRUST env, validation runs, unit
  tests, debugging env failures.
- **Key commands:** `source env_trust_LiMe.sh`,
  `.opencode/scripts/with-trust-env.sh <cmd>`,
  `.opencode/scripts/wait-build.sh`.
- **Hosts:** laptop (T430s, 4 threads, HDD) vs workstation (Ryzen 5 5600X, SSD).

### `trust-lime-git`
Git hygiene for trust-LiMe.
- **When to use:** staging/committing, branches/worktrees, reviewing diffs,
  reconciling uncommitted work across the two repos.
- **Key points:** `trust-LiMe` and `trust-code` are separate repos — never
  mix their changes; commit kernel first when a module change depends on it;
  group by intent.

### `trust-lime-notebooks`
Author, edit, and run Jupyter validation notebooks and `.data` datasets.
- **When to use:** touching any `.ipynb` or `.data` file, or running
  validation.
- **Key points:** trustutils pipeline (`addCase`/`runCases`/`tablePerf`),
  sequential-by-default execution (parallel only on allow-listed paths),
  `check-validation.sh` gate, baseline harness.

### `trust-lime-testing`
Run and extend tests.
- **When to use:** writing/running tests, triaging failures, choosing build
  targets, staging validation cases.
- **Key commands:** `.opencode/scripts/syntax-check.sh src/X.cpp`,
  `.opencode/scripts/unit-tests.sh [--arith-only]`,
  `make check_optim` / `ctest_optim` (and `_semi_optim`, `_debug` variants).
- **Note:** dataset/notebook authoring lives in `trust-lime-notebooks`.

## 2. Standard workflow

### Agent roles
- `build` (default, `opencode/beam-free`): day-to-day editing and builds.
- `@long` (`opencode/beam-free`): large multi-file analysis.
- `@quick` (`opencode/deepseek-v4.1-flash-free`): small questions.
- Subagents (`@general`, `@explore`, `@long`, `@quick`) for parallel work.

### Plan vs. build mode
- Start in **Plan** mode to discuss and design; switch to **Build** mode to
  implement. Do not edit files while in Plan mode.

### Primary loop
1. Edit source.
2. `.opencode/scripts/syntax-check.sh src/File.cpp` (fast, C++20).
3. `make module_semi_optim` (default build target).
4. Run validation or unit tests as appropriate.
5. Commit by intent (see `trust-lime-git`).

### Environment
- Source via `.opencode/scripts/with-trust-env.sh <cmd>` rather than manual
  `source` lines.
- Poll long builds with `.opencode/scripts/wait-build.sh`.
- Keep the Headroom proxy at `--mode cache`.

### Retrieval
- `KERNEL_REUSE_NOTES` is now `docs/content/dev_corner/kernel_reuse.md`.
- Use `.opencode/scripts/retrieve-notes.sh <topic words>` for targeted
  retrieval from the knowledge base, plans, and docs.
- Use `/find-symbol` for TRUST/TrioCFD symbol lookup.

### Validation rules (enforced by `check-validation.sh`)
- `.ipynb` at form root only; one form per notebook.
- All cases sequential by default (no `Decouper`/`Partitionneur`/`Scatter`,
  no `partition()`, no `nbProcs>1`).
- Parallel exception only for allow-listed paths, with the three marker
  regions and `nbProcs=2` + `case.partition()`.

### Commit hygiene
- Group by intent, not file count.
- Keep `trust-LiMe` and `trust-code` commits separate.
- Run syntax check and (when relevant) unit tests before committing.
