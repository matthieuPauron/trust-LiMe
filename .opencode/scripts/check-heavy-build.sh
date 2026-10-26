#!/usr/bin/env bash
# check-heavy-build.sh — guard for RAM-intensive builds (sanitizer/module_custom,
# full-kernel builds). Exits 0 when it is safe to proceed, non-zero (with a
# reason on stderr) otherwise.
#
# Policy:
#   - Host detection: workstation (matthieu-precisiont5610 / >=8 threads) vs
#     laptop (T430s-class, <=4 threads). On the laptop, heavy builds are
#     refused outright unless TRUST_ALLOW_HEAVY_BUILD=1 (interactive override).
#   - Daytime (08:00-20:00 local): heavy builds are blocked unless
#     TRUST_ALLOW_HEAVY_BUILD=1 is set (interactive confirmation override).
#   - Free RAM must be at least MIN_FREE_GB (default 6) unless overridden.
#
# Usage:
#   .opencode/scripts/check-heavy-build.sh            # check only
#   TRUST_ALLOW_HEAVY_BUILD=1 .opencode/scripts/check-heavy-build.sh
#
# Exit codes: 0 ok, 1 blocked (reason on stderr), 2 usage/internal error.

set -euo pipefail

MIN_FREE_GB="${MIN_FREE_GB:-6}"

# --- host detection ---------------------------------------------------------
hostname="$(hostname 2>/dev/null || echo unknown)"
nthreads="$(nproc 2>/dev/null || echo 1)"
host_class="unknown"
case "$hostname" in
  *precisiont5610*|*workstation*) host_class="workstation" ;;
  *t430*|*laptop*) host_class="laptop" ;;
  *)
    if [ "$nthreads" -ge 8 ]; then host_class="workstation"; else host_class="laptop"; fi
    ;;
esac

hour=$(date +%H)
daytime=0
if [ "$hour" -ge 8 ] && [ "$hour" -le 19 ]; then daytime=1; fi

free_gb=$(free -g | awk '/^Mem:/ {print $7}')
if [ -z "$free_gb" ]; then free_gb=0; fi

override="${TRUST_ALLOW_HEAVY_BUILD:-0}"

blocked=0
reasons=()

if [ "$host_class" = "laptop" ] && [ "$override" != "1" ]; then
  blocked=1
  reasons+=("laptop host (${hostname}, ${nthreads} threads): heavy builds refused outright")
fi

if [ "$daytime" -eq 1 ] && [ "$override" != "1" ]; then
  blocked=1
  reasons+=("daytime (local hour ${hour}): heavy builds require explicit confirmation")
fi

if [ "$free_gb" -lt "$MIN_FREE_GB" ] && [ "$override" != "1" ]; then
  blocked=1
  reasons+=("free RAM ${free_gb} GB < ${MIN_FREE_GB} GB threshold")
fi

if [ "$blocked" -eq 1 ]; then
  {
    echo "BLOCKED: RAM-heavy build guard triggered (${host_class} host):"
    for r in "${reasons[@]}"; do echo "  - $r"; done
    echo "Set TRUST_ALLOW_HEAVY_BUILD=1 after explicit user confirmation to override."
  } >&2
  exit 1
fi

echo "OK: heavy build allowed (host=${host_class}, hour=${hour}, free=${free_gb}GB, override=${override})."
exit 0