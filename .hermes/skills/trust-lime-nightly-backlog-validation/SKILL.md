---
name: trust-lime-nightly-backlog-validation
description: Use when validating trust-LiMe nightly backlog tasks.
version: 1.2.0
author: Hermes Agent
license: MIT
platforms: [linux]
metadata:
  hermes:
    tags: [trust-lime, backlog, nightly, cron, validation]
    related_skills: []
---

## When to Use

- The user says a nightly backlog task is validated/approved or asks for rework.
- The user asks about the status of the nightly backlog job or current task.
- After the `trust-lime-nightly-backlog` cron job has run and produced output.

# Nightly backlog task validation protocol

The cron job `trust-lime-nightly-backlog` (id `a409d93585ae`) runs nightly at 23:00, reconciles the backlog with the plan files, then executes a *batch* of highest-priority tasks from `docs/content/NEXT.md` (section 3, "Prescribed for the next weeks") as long as no file is edited by two distinct tasks. It records state in `/home/matthieu/.hermes/nightly-backlog-state.json`.

## State file schema

```json
{"validated_tasks": [titles...], "current_tasks": [titles...], "status": "idle"|"in_progress"|"awaiting_validation"|"failed", "started_at": ISO|null, "last_run_summary": string, "edited_files": [paths...], "worktrees": [{"task": title, "path": "/abs/path", "branch": "name"}...]}
```

A run may execute a *batch* of tasks (multiple) as long as no file is edited by two distinct tasks. `current_tasks` and `edited_files` describe that batch.

## When the user validates a completed task

1. Read `/home/matthieu/.hermes/nightly-backlog-state.json`.
2. Confirm `current_tasks` (a batch) and that status is `awaiting_validation` (or `failed` if retrying).
3. Append all titles in `current_tasks` to `validated_tasks`.
4. Set `current_tasks` = [], `status` = `idle`, `edited_files` = [], keep `last_run_summary` (or update with validation note).
5. Write state file.
6. Optionally update `docs/content/NEXT.md`: move the validated tasks from section 3 to section 1 (Completed) with evidence. Only on explicit user request or clear validation.
7. Report to the user which tasks are now validated and what the next nightly run will pick.

## When the user rejects / requests rework

1. Read state file.
2. Set `status` = `failed` (or keep `awaiting_validation`), update `last_run_summary` with the user's feedback.
3. Do NOT add to `validated_tasks`.
4. Optionally the user may ask to retry; set `status` = `idle` and `current_tasks` = [] so the next run re-picks the same tasks (since they are not in validated_tasks).

## Reconciliation performed by the nightly job

Each run first reconciles `docs/content/NEXT.md` with `docs/content/plans/*.md`: it adds any plan task missing from the backlog and marks backlog tasks that are already done (with evidence) as completed. Task execution itself does not mark its own task done; only validation does.

## When the user asks for status

Read state file and report current_tasks (batch), status, last_run_summary, edited_files, and the next prescribed task that would be picked (first in section 3 not in validated_tasks).

## Pitfalls

- Do not fabricate an analytical solution for a nonlinear coupled transport case (mode-mixing advection plus k-ε sources). Use convergence, positivity, and finiteness checks, and keep the existing manufactured-term and analytic-decay checks as the analytic anchors. State this limitation plainly rather than implying an analytic comparison exists.
- Estimate each candidate task's likely file set before editing; stop the batch before any task whose files overlap an already-edited file. Post-hoc detection alone is too late — the conflicting edit has already happened.
- When marking a task done in `NEXT.md`, move it to "Completed (recent)" with evidence and renumber the prescribed list; keep dependency order. Do not leave the completed task in the prescribed list. If the task was only partially completed, mark the Completed entry "(partial)" and keep the remaining sub-items in the "In progress" row.
- Reconciliation edits to `NEXT.md` are expected and allowed; task execution does not mark its own task done — only user validation does.
- State-file writes are guarded: a write is refused unless the file was read in the same session. Always read before writing, or use patch for small edits. Never overwrite from a stale copy.
- The patch tool's fuzzy (context_aware) matching can introduce stray trailing characters (e.g. a trailing `"`). After every patch, inspect the diff/output and clean up artifacts with a small sed/python pass rather than leaving them.
- When a task modifies files, record every path in `edited_files` in the same state write; an empty `edited_files` after a mutating task is a tracking defect, not "no edits".
- A run that ends without a final response (timeout/interruption) is not a validation: leave `status` as `in_progress`, do not append to `validated_tasks`, and let the next run reconcile. Never infer completion from partial work or a built-but-untested binary.
- Environment-altering tasks (e.g. sanitizer builds that flip `project.cfg` flags) leave the working tree dirty; either complete the cleanup in the same task or record it in `last_run_summary`. A leftover sanitizer config contaminates subsequent normal builds.
- When reverting an interrupted run, restore the modified repo files to HEAD (do not leave sanitizer flags or reconciliation edits in place) and reset state to `idle` with a `last_run_summary` noting the revert; do not silently leave `status: in_progress` once the working tree is clean.
- RAM-heavy builds (`make module_custom`, full-kernel builds) can exhaust RAM on either host (laptop or workstation) and crash the machine. The nightly job runs at 23:00 (night) so it may run them, but only with background builds + generous timeouts and never two concurrently. The job prompt's RESOURCE GUARD defers a RAM-heavy candidate during daytime (08:00-20:00) or when free RAM < ~6 GB and records the deferral. Interactive sessions must warn the user and require explicit confirmation before starting such a build.
- Helper `.opencode/scripts/check-heavy-build.sh` exits non-zero (with a reason) unless `TRUST_ALLOW_HEAVY_BUILD=1` is set; use it as a pre-check before any heavy build.
- Sanitizer configure: the fftw prerequisite test fails to link because prebuilt fftw lacks the sanitizer runtime (undefined `__ubsan_handle_*` / `__asan_report_*`); bypass with `TRUST_SKIP_PREREQUISITE_TEST=1` and document the bypass in the job prompt. Do not silently drop the prerequisite check without recording it.
- Sanitizer flags must be enabled in pairs: set both `cpp_flags` and `ld_flags` in `project.cfg`. Enabling only `cpp_flags` leaves compile instrumentation without the link-time runtime and breaks the build. Revert both after the session.
- Long builds on the HDD host time out at 180 s in the foreground; run them in the background with notification and a generous timeout, and never retry the same short foreground call. Two consecutive 180 s timeouts are a signal to switch to background, not to retry.
- Cron runs cannot ask questions; any workaround (skip flags, env vars, prerequisite-test bypasses) must be fully specified in the job prompt or in a skill the job loads — never assumed from chat context.
- The nightly job itself does not commit. A commit happens only when the user explicitly asks ("commit the validation"); then stage the notebook, data files, NEXT.md, and validation status together.

## Constraints

- Never let the cron job pick a second task while one is awaiting validation; the state machine enforces this.
- The nightly job does not commit or push; it leaves changes in the working tree. A commit happens only on explicit user request during validation.
- Cron runs are autonomous and cannot ask questions; all decisions must be documented in last_run_summary.
- Interactive sessions follow the same commit discipline: `.opencode/validation_status.json` is committed whenever it changes (cron runs do not commit).


## Worktrees (nightly job, updated 2026-10-26)

The nightly job now runs each task in its own git worktree (see
`trust-lime-worktree`) so tasks that touch overlapping files no longer block
each other. State tracks them in `worktrees` (list of `{task, path, branch}`).
Changes live in those worktrees; the main working tree stays clean until the
user reviews and merges.

- When the user validates a completed batch, also remove the worktrees
  (`git worktree remove <path>`) or keep them for further inspection.
- `edited_files` is typically `[]` for a worktree-based run; use `worktrees`
  to locate the changes.
- The job attaches `trust-lime-worktree` and `trust-lime-build-diagnostics`
  skills; load them when reasoning about worktree creation or heavy builds.


## Agent-proposed tasks (manual validation required)

The nightly job may append follow-up tasks to section 4 of `docs/content/NEXT.md`
("Proposed by agent (awaiting validation)") after completing a task. These are
**never** executed by the cron job until a human validates them.

### When the user validates a proposed task

1. Read `docs/content/NEXT.md` section 4.
2. On approval: move the entry to section 3 ("Prescribed for the next weeks") with
   an appropriate priority, or to section 2 ("In progress") if partially done.
   Remove it from section 4.
3. Update `~/.hermes/nightly-backlog-state.json`: remove the title from
   `proposed_tasks` (if present). Do NOT add to `validated_tasks` unless the
   task has actually been executed and verified.
4. Report which proposals were promoted and that the next nightly run may now
   pick them up.

### When the user rejects a proposal

1. Remove the entry from section 4 (or mark it rejected with a note).
2. Remove from `proposed_tasks` in state.
3. Do not promote to section 3.

### Pitfalls

- A proposal is not evidence of completion; do not move it to "Completed".
- Only the user can promote a proposal; the cron job must never auto-promote.
- Keep the Proposed section empty when there are no pending proposals.


## Shared worktrees (multiple tasks)

When the nightly job places several directly-following tasks in the same
worktree, the worktree entry has `tasks: [title1, title2, ...]`. Each task
requires its own validation:

- Do not mark the worktree validated until every task in `tasks` is validated.
- When validating, the user may validate them one at a time or together; if
  together, append all titles to `validated_tasks` and clear `current_tasks`.
- On rejection of one task, keep the others in `current_tasks` (or split the
  worktree) and record feedback in `last_run_summary`.
- A reused worktree keeps a single branch/path; the transition between tasks
  is documented in `last_run_summary`.
