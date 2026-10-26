---
name: trust-LiMe OpenCode delegation
description: >
  Delegate coding work from Hermes to OpenCode in the trust-LiMe repo: pin the
  model, avoid push, keep the build loop. Use when an unattended or delegated
  job needs to edit, build, or triage in this repository.
---

# trust-LiMe OpenCode delegation

## Model pinning

Some models reachable via the NVIDIA endpoint are unprovisioned or hang. Always
pin a known-good model on delegated runs:

```bash
opencode run --provider gemini --model gemini-3.1-flash-lite "..."
```

## Rules

- **Never let it push.** The project denies `git push` to agent sessions; keep
  that policy in the delegated prompt.
- **Pin the model** on every delegated call (section above).
- **Run from the repo root** so relative paths (`.opencode/scripts/...`,
  `src/`, `share/...`) resolve.
- **Keep the build loop**: `syntax-check.sh` -> `make module_semi_optim` ->
  validate. The delegated prompt should state this.

## Typical delegated tasks

- Triage a failing test or dataset.
- Open a worktree and prepare a patch (not a push).
- Run a targeted validation case and report probe values.

## Project binding

```bash
hermes project show trust-lime   # folders: trust-LiMe + trust-code
```

The binding includes the sibling `trust-code` checkout, so delegated jobs can
read kernel base classes.

## Pitfalls

- Free-tier exhaustion: both primary and fallback are free lanes; keep
  delegated work to scheduled/unattended jobs, not interactive bursts.
- The self-improvement review only completes in a long-lived session, not in
  `-z` one-shots.
- `hermes pause` silences all scheduled and gateway activity during long
  rebuilds.
