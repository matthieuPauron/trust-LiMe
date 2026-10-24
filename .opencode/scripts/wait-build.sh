#!/usr/bin/env bash
# Wait for a background build to finish, then report.
#
# Usage:
#   .opencode/scripts/wait-build.sh [pattern] [poll_seconds]
#
# pattern: ERE matched against full command lines
#          (default: 'make (optim|semi_optim|module_|check_|ctest_)')
# poll_seconds: sleep between checks (default: 5)
set -u

pattern="${1:-make (optim|semi_optim|module_|check_|ctest_)}"
interval="${2:-5}"

# Print PIDs whose command line matches pattern, excluding this script and
# its helper processes.
matching_pids() {
  local pid cmd
  for d in /proc/[0-9]*; do
    pid="${d##*/}"
    [ "$pid" = "$$" ] && continue
    [ -r "$d/cmdline" ] || continue
    cmd="$(tr '\0' ' ' < "$d/cmdline" 2>/dev/null)" || continue
    [ -z "$cmd" ] && continue
    case "$cmd" in
      *wait-build.sh*|*pgrep*|*matching_pids*) continue ;;
    esac
    if printf '%s' "$cmd" | grep -qE -- "$pattern"; then
      printf '%s\n' "$pid"
    fi
  done
}

while true; do
  pids="$(matching_pids)"
  if [ -z "$pids" ]; then
    break
  fi
  echo "still running: $pattern  (pids: $(printf '%s' "$pids" | tr '\n' ' '))"
  sleep "$interval"
done

echo "done: $pattern"