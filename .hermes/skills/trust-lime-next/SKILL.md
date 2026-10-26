---
name: TRUST-LiMe next tasks
description: >
  Show the next tasks from the TRUST-LiMe backlog when the user asks
  "What's next to do ?", "what's next", "next tasks", "backlog", "roadmap",
  "what should I work on", or similar. Read the living backlog and current
  project open items, then present outstanding work in priority order. Use
  whenever the user asks what to work on next.
---

# TRUST-LiMe next tasks

## Sources

Use these sources in order:

1. `docs/content/NEXT.md` — authoritative living backlog, especially "In
   progress" and "Prescribed for the next weeks".
2. `AGENTS.md` — the concise "Open items" section for current project state.
3. `docs/content/plans.md` — sections 3–4 for near-term detail and dependencies.
4. `docs/content/end_goal.md` — only when the user asks about the long-term
   roadmap.

## Procedure

1. Read `docs/content/NEXT.md` and the "Open items" section of `AGENTS.md`.
2. Reconcile overlapping entries; use `NEXT.md` for ordering and more detailed
   status. Consult `plans.md` when useful for clarifying scope or references.
3. Check `git log --oneline -10` for recently landed work. If the backlog may be
   stale, call that out rather than silently removing or rewriting an item.
4. Present the outstanding work from the backlog. Do not infer or invent new
   tasks. Keep long-term goals separate from immediate next steps.

## Response format

- Start with **Next tasks** and identify `docs/content/NEXT.md` as the source.
- Give a concise, numbered priority list. For each task, include its status
  (in progress, open, or planned), a short description of the remaining work,
  and a useful file/section reference when available.
- Briefly summarize partially complete work and its outstanding pieces.
- Add a short **Recently completed** note when it helps explain the current
  priorities; don't repeat the completed backlog.
- Finish by offering to explore a selected task (plan, code location, or
  validation case).
- Aim for roughly 15–25 lines. Do not dump the source documents.

If backlog sources are unavailable, say so. If sources conflict, report the
discrepancy and point to the relevant files instead of presenting uncertain
status as fact.


## Hermes usage

In Hermes there are no OpenCode slash commands. Run the scripts above via the
`terminal` tool (paths are repo-relative), read/edit files with `read_file` /
`patch` / `write_file`, use `execute_code` for bulk checks, and `delegate_task`
for parallel analysis. Heavy builds must pass `.opencode/scripts/check-heavy-build.sh`
and follow the RAM guard in `AGENTS.md`.
