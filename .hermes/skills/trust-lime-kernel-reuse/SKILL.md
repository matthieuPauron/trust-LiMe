---
name: trust-LiMe kernel reuse
description: >
  Retrieve and extend the accumulated kernel-reuse knowledge base
  (docs/content/dev_corner/kernel_reuse.md, 1700+ lines). Use before extending
  the module, when choosing kernel base classes, or when appending a new
  finding.
---

# trust-LiMe kernel reuse

## Retrieval (prefer targeted over full reads)

```bash
.opencode/scripts/retrieve-notes.sh <topic words>   # best-matching sections
.opencode/scripts/retrieve-notes.sh --list           # browse headings
.opencode/scripts/retrieve-notes.sh --top N --max-lines M
```

The file is 1700+ lines; do not read it whole unless explicitly needed.

## Before extending

1. Read the relevant sections (retrieve by topic).
2. Inspect the Doxygen inheritance graphs of `Probleme_base` / `Equation_base`
   and derive from the closest class (e.g. `Convection_Diffusion_Temperature`).
3. Reuse kernel base classes rather than reimplementing; kernel sources are
   `trust-kernel` / `trust-include` references (`../trust-code`).

## After extending

Append new findings to `docs/content/dev_corner/kernel_reuse.md` so the next
session does not re-derive them. Keep entries as concrete recipes, not
narration.

## Symbol lookup in the kernel

```bash
.opencode/scripts/find-symbol.sh <name> [--context N] [--trio] [--count]
.opencode/scripts/index-codebases.sh both --force   # rebuild indexes
```

Indexes live in `.index/` (gitignored); rebuild after a `git pull` of
`trust-code` / `triocfd-code`.

## Pitfalls

- GNU ctags is syntactic, not semantic: templates/macros may be missed.
- ctags indexes definitions/declarations only; use ripgrep for usages.
- `find-symbol.sh` warns if the index is stale (disable with
  `TRUST_INDEX_NO_CHECK=1`).
