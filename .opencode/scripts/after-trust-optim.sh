#!/usr/bin/env bash
# after-trust-optim.sh — wait for the TRUST kernel to be built in optim mode,
# then run every step that was blocked on it.
#
# Detection: the standard kernel archive $TRUST_ROOT/lib/TRUST_mpi_opt.a is the
# last artefact of `make optim` (it merges micro + numeric + standard). It must
# exist *and* keep the same size across two consecutive polls, so a half-written
# archive does not trigger a premature run.
#
# Once detected, in order:
#   1. ./configure          - baltik re-globs src/, so the source term added
#                            after the last configure is compiled and linked
#   2. make module_optim    - same flavour as the kernel; baltik derives the
#                            kernel archive name from the mode, so a
#                            module_semi_optim cannot link against an _opt kernel
#   3. unit-tests.sh        - the suites that need the standard kernel
#   4. validation notebooks     - MagneticStandalone/*/<FormName>.ipynb (one per
#                            case), exported to PDF and concatenated into
#                            MagneticStandalone_validation.pdf at the repo root
#
# Usage: bash .opencode/scripts/after-trust-optim.sh [--once] [--mode=optim|semi_optim]
#        --once  check immediately instead of polling every minute
#        --mode kernel flavour to build the module against (default: same as the
#               kernel that was detected, i.e. "optim")
set -uo pipefail

repo=/home/matthieu/Development/trust-LiMe
trust=/home/matthieu/Development/trust-code
marker="$trust/lib/TRUST_mpi_opt.a"
interval=60
max_wait=$((12 * 3600))   # give up after 12h
nb_log=notebooks
mode=optim

for arg in "$@"; do
  case "$arg" in
    --once) ;;
    --mode=*) mode="${arg#--mode=}" ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done
if [[ "$mode" != "optim" && "$mode" != "semi_optim" ]]; then
  echo "--mode must be 'optim' or 'semi_optim'" >&2; exit 2
fi
mkdir -p /tmp/opencode/nb_log

say() { printf '\n=== [%s] %s\n' "$(date '+%H:%M:%S')" "$*"; }

# ---------------------------------------------------------------- detection --
say "watching for $marker (poll every ${interval}s, give up after $((max_wait/3600))h)"
waited=0
prev_size=""
while true; do
  if [[ -f "$marker" ]]; then
    size=$(stat -c %s "$marker")
    # If the archive has not been touched for a while it is complete: this is
    # what makes a restart of this script instant instead of costing a poll.
    age=$(( $(date +%s) - $(stat -c %Y "$marker") ))
    if [[ $age -gt 90 ]]; then
      say "TRUST optim detected: $marker ($(numfmt --to=iec "$size")), idle ${age}s"
      break
    fi
    if [[ "$size" == "$prev_size" ]]; then
      say "TRUST optim detected: $marker ($(numfmt --to=iec "$size")), stable"
      break
    fi
    say "found $marker (${size} bytes), waiting for it to stop growing..."
    prev_size="$size"
  fi
  if [[ "$waited" -ge "$max_wait" ]]; then
    say "TIMEOUT after ${waited}s; TRUST optim never appeared. Giving up."
    exit 1
  fi
  sleep "$interval"
  waited=$((waited + interval))
done

# ------------------------------------------------------------------- step 1 --
# TRUST_ROOT must exist before ./configure: baltik_CONFIGURE reads it, and
# env_TRUST.sh also exports the compiler wrappers the later steps rely on.
say "initialising the TRUST environment"
# shellcheck disable=SC1091
source "$trust/env_TRUST.sh" > /tmp/opencode/env_trust.log 2>&1
if [[ -z "${TRUST_ROOT:-}" ]]; then
  say "env_TRUST.sh did not export TRUST_ROOT; falling back to $trust"
  export TRUST_ROOT="$trust"
fi

say "STEP 1/4: ./configure (re-glob src/ so the source term is picked up)"
cd "$repo" || exit 1
./configure > /tmp/opencode/configure.log 2>&1
rc=$?
if [[ $rc -ne 0 ]]; then
  say "./configure FAILED (rc=$rc); tail of /tmp/opencode/configure.log:"
  tail -30 /tmp/opencode/configure.log
  exit 1
fi
say "./configure ok"
if grep -q 'Terme_Source_Induction_stretching' build/src/.liste_sources_atelier 2>/dev/null; then
  say "the new source is registered in the build"
else
  say "WARNING: Terme_Source_Induction_stretching is still absent from the build"
fi

# ------------------------------------------------------------------- step 2 --
# The module target must match the kernel that was built: baltik derives the
# kernel archive name from the mode (baltik_OPT_management: optim -> _opt,
# semi_optim -> _semi_opt), so "make module_semi_optim" cannot link against a
# kernel that only exists as libTRUST_mpi_opt.a.
if [[ "$mode" == "semi_optim" ]]; then
  module_target=module_semi_optim
else
  module_target=module_optim
fi
say "STEP 2/4: make $module_target (slow: HDD, 4 threads)"
source ./env_trust_LiMe.sh > /tmp/opencode/env_module.log 2>&1
make "$module_target" > /tmp/opencode/module_build.log 2>&1
rc=$?
if [[ $rc -ne 0 ]]; then
  say "make $module_target FAILED (rc=$rc); errors:"
  grep -nE 'error:|Error [0-9]|Could not find' /tmp/opencode/module_build.log | head -30
  exit 1
fi
say "module built"
ls -la "$repo"/trust-lime* 2>/dev/null || say "WARNING: no trust-lime* executable found"

# ------------------------------------------------------------------- step 3 --
say "STEP 3/4: unit tests (full suite, incl. the ones needing the standard kernel)"
bash .opencode/scripts/unit-tests.sh > /tmp/opencode/unit_tests_full.log 2>&1
rc=$?
tail -25 /tmp/opencode/unit_tests_full.log
say "unit tests rc=$rc  (full log: /tmp/opencode/unit_tests_full.log)"

# ------------------------------------------------------------------- step 4 --
say "STEP 4/4: validation notebooks -> per-form PDFs -> concat report at repo root"
source "$trust/env_TRUST.sh"      > /dev/null 2>&1
source "$trust/env_for_python.sh" > /dev/null 2>&1
# shellcheck disable=SC1091
source /home/matthieu/Development/venv_trust/bin/activate
bash .opencode/scripts/run-validation-pdf.sh > /tmp/opencode/notebook_pdf.log 2>&1
rc=$?
tail -30 /tmp/opencode/notebook_pdf.log
say "notebooks+pdf rc=$rc  (log: /tmp/opencode/notebook_pdf.log)"
if [[ $rc -ne 0 ]]; then
  say "at least one notebook failed (see above)"
else
  say "notebooks executed + concatenated into $repo/MagneticStandalone_validation.pdf"
fi

say "ALL STEPS DONE (configure=$?, module=ok, unit_tests=$rc, notebook=$rc)"