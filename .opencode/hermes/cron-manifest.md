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
