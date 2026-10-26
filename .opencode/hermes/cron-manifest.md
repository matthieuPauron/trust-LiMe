# Hermes cron manifest for trust-LiMe

Run `.opencode/scripts/install-hermes.sh` to (re)create these jobs.
Scripts are copied (not symlinked) from `.opencode/hermes/scripts/` into `~/.hermes/scripts/` — Hermes rejects symlinks that escape the scripts dir.

## Active jobs

### trust-lime-docs-coherence
```
hermes cron create "30 2 * * *" \
  --name trust-lime-docs-coherence \
  --script trust-lime-docs-coherence.sh \
  --no-agent \
  --deliver local \
  --workdir /home/matthieu/Development/trust-LiMe
```

### trust-lime-validation-report
```
hermes cron create "0 1 * * *" \
  --name trust-lime-validation-report \
  --script trust-lime-validation-report.sh \
  --no-agent \
  --deliver local \
  --workdir /home/matthieu/Development/trust-LiMe
```

### trust-lime-nightly-backlog
```
hermes cron create "0 23 * * *" \
  --name trust-lime-nightly-backlog \
  --skill trust-lime-worktree \
  --skill trust-lime-build-diagnostics \
  --deliver local \
  --workdir /home/matthieu/Development/trust-LiMe
```
Note: this job is agent-driven (no --script/--no-agent). Its prompt is maintained via
`hermes cron edit a409d93585ae --prompt ...` (see the job's current prompt in
`~/.hermes/cron/jobs.json`). Each task runs in its own worktree under
`/home/matthieu/Development/trust-LiMe-worktrees/`.
