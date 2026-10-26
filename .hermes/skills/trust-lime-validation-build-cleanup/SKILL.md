---
name: trust-LiMe validation build cleanup
description: >
  Remove generated build directories created by validation notebooks, on
  explicit request. Scope is limited to
  share/Validation/Rapports_automatiques/**/build.
---

# Validation notebook build cleanup

Use this skill only when explicitly asked to clean validation notebook build
outputs. Never run cleanup automatically after executing a notebook.

## Scope and safety

- Target only directories named `build` beneath
  `share/Validation/Rapports_automatiques/`.
- Never remove the repository-level `build/`, `docs/build/`, `src/`, notebook
  source files, or any path outside the validation tree.
- Do not follow symlinks while discovering targets. If the validation root is
  missing or resolves outside the repository, stop.
- Before deleting, display the full paths and total count. Ask for confirmation
  unless the user's request already clearly authorizes deletion of this exact
  scope.
- Do not remove `.son`, `.stop`, logs, or other run artifacts outside the
  matched `build/` directories.

## Procedure

1. Resolve the repository root and validation root. Verify that the validation
   root is exactly `<repo>/share/Validation/Rapports_automatiques` after
   canonicalization.
2. Discover matching directories without traversing symlinks, pruning each
   `build` match so nested contents are not reported as separate targets:

   ```bash
   find "$validation_root" -type d -name build -prune -print0
   ```

3. Preview the exact targets and count them. If none are found, report that
   there is nothing to clean.
4. After the required confirmation, remove only the previewed directories.
   Preserve the NUL-delimited paths when handling them; do not use an
   unquoted wildcard or broaden the removal scope.
5. Report the number of directories removed and any failures.


## Hermes usage

In Hermes there are no OpenCode slash commands. Run the scripts above via the
`terminal` tool (paths are repo-relative), read/edit files with `read_file` /
`patch` / `write_file`, use `execute_code` for bulk checks, and `delegate_task`
for parallel analysis. Heavy builds must pass `.opencode/scripts/check-heavy-build.sh`
and follow the RAM guard in `AGENTS.md`.
