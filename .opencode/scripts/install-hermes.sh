#!/usr/bin/env bash
# Install/sync trust-LiMe Hermes assets into ~/.hermes/.
# 1. Copies scripts from .opencode/hermes/scripts/ into ~/.hermes/scripts/ (Hermes rejects symlinks that escape).
# 2. (Re)creates the cron jobs listed in .opencode/hermes/cron-manifest.md.
set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$REPO/.opencode/hermes/scripts"
DST="$HOME/.hermes/scripts"

mkdir -p "$DST"
for f in "$SRC"/*.sh; do
  [ -f "$f" ] || continue
  name="$(basename "$f")"
  cp -f "$f" "$DST/$name"
  chmod +x "$f"
  echo "linked: $name"
done

job_id_by_name() {
  local name="$1" id="" line
  while IFS= read -r line; do
    case "$line" in
      *"["*"]"*) id="$(echo "$line" | awk '{print $1}')" ;;
      *"Name:"*)
        if echo "$line" | grep -q "$name"; then echo "$id"; return 0; fi
        ;;
    esac
  done < <(hermes cron list 2>/dev/null)
  return 1
}

recreate_job() {
  local name="$1"; shift
  local existing
  existing="$(job_id_by_name "$name" 2>/dev/null || true)"
  if [ -n "$existing" ]; then
    hermes cron remove "$existing" >/dev/null 2>&1 || true
    echo "removed existing: $name ($existing)"
  fi
  echo "creating: $name"
  "$@" 2>&1 | tail -3
}

echo ""
echo "== cron jobs =="
recreate_job "trust-lime-docs-coherence" \
  hermes cron create "30 2 * * *" \
    --name trust-lime-docs-coherence \
    --script trust-lime-docs-coherence.sh \
    --no-agent \
    --deliver local \
    --workdir "$REPO"

recreate_job "trust-lime-validation-report" \
  hermes cron create "0 1 * * *" \
    --name trust-lime-validation-report \
    --script trust-lime-validation-report.sh \
    --no-agent \
    --deliver local \
    --workdir "$REPO"

echo ""
echo "done. Run 'hermes cron list' to verify."
