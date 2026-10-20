#!/usr/bin/env bash
# notebook-baseline.sh — RULE-enforcement harness for validation-notebook edits.
#
# The RULE (AGENTS.md): an operation on a notebook is validated if the
# computations of the notebook generate the same result as before the
# operation.
#
# Usage (any cwd; paths resolved from the repository root):
#   notebook-baseline.sh execute <Category>/<Form>
#       Run the CURRENT state of the form's notebook headless (nbconvert) and
#       snapshot the executed notebook + the baseline case's probe files
#       (build/PAR_*.son and build/coarse/PAR_*.son — the coarse scale IS the
#       baseline mesh) under /tmp/opencode/lime_baseline/<Category>_<Form>/.
#       Call this BEFORE touching the notebook (or its src/ datasets).
#
#   notebook-baseline.sh compare <Category>/<Form>
#       Re-run the notebook as it is NOW and compare against the snapshot:
#         - every unchanged code cell must print identical output
#           (tablePerf cells skipped: timings vary);
#         - the baseline PAR_*.son probes must be reproduced exactly;
#         - the run/scan registration cells may change (their computation is
#           covered by the .son comparison); any other changed cell must
#           reproduce its computation (stream text, with the historical
#           L0/L1/L2 mesh-scale labels mapped to coarse/standard/fine).
#       Exit 0 = operation validated, 1 = regression or run failure.
#
# Notes:
#   - Runs in the TRUST Python environment (venv_trust), with the trust-LiMe
#     module binary ($exec must be trust-lime*).
#   - Deletes only git-ignored run artifacts (src/*.son, *.stop, *.TU, ...)
#     before executing, so the suite's `cp -a src/* build/` copies a pristine
#     dataset directory (a leftover .stop would truncate the solve).
# Note: no `set -u` — the sourced TRUST env scripts rely on unbound-by-default
# variables (TRUST_ROOT etc.).
set -o pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$script_dir/../.." && pwd)

usage() {
  sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
}

[ $# -eq 2 ] || usage
cmd="$1"
form="$2"
case "$cmd" in
  execute|compare) ;;
  *) echo "notebook-baseline: unknown command: $cmd" >&2; usage ;;
esac

formdir="$repo/share/Validation/Rapports_automatiques/$form"
nb="$formdir/$(basename "$form").ipynb"
[ -f "$nb" ] || { echo "notebook-baseline: no notebook at $nb" >&2; exit 2; }

tag=${form//\//_}
base=/tmp/opencode/lime_baseline/$tag

# --- TRUST python environment + module binary -------------------------------
source "$repo/../trust-code/env_TRUST.sh"
source "$repo/../trust-code/env_for_python.sh"
source "$repo/env_trust_LiMe.sh"
source ~/Development/venv_trust/bin/activate

if [ -n "${exec:-}" ] && [[ "$exec" == *trust-lime* ]]; then
  :
else
  echo "notebook-baseline: WRONG exec: ${exec:-<unset>} (need the trust-lime binary)" >&2
  exit 2
fi

# --- clean stale, git-ignored run artifacts ----------------------------------
clean_artifacts() {
  find "$formdir/src" -maxdepth 1 -type f \
    \( -name '*.son' -o -name '*.sons' -o -name '*.stop' -o -name '*.out' \
       -o -name '*.err' -o -name '*.TU' -o -name '*.log' -o -name '*.dt_ev' \
       -o -name '*.progress' -o -name '*.sauv' -o -name '*.lml' -o -name '*.prm' \) \
    -delete
  rm -rf "$formdir/build"
}

run_nbconvert() {  # $1 = output directory, $2 = output basename
  ( cd "$formdir" && jupyter nbconvert --to notebook --execute "$nb" \
      --output-dir "$1" --output "$2" \
      --ExecutePreprocessor.timeout=900 ) >> "$1/execute.log" 2>&1
}

case "$cmd" in
  execute)
    rm -rf "$base"
    mkdir -p "$base"
    clean_artifacts
    echo "notebook-baseline: executing $form (snapshot) ..."
    if ! run_nbconvert "$base" executed.ipynb; then
      echo "notebook-baseline: FAILED to execute $form (see $base/execute.log)" >&2
      exit 1
    fi
    # Baseline probes: build root (extra non-scan cases, e.g. a merged
    # two-way twin) + build/coarse/ (the coarse scale IS the baseline mesh)
    # + build/<Case>/coarse/ (the per-case layout of the merged MMS forms).
    for d in "$formdir/build" "$formdir/build/coarse" "$formdir/build"/*/coarse; do
      cp "$d"/*.son "$base/" 2>/dev/null || true
    done
    n_son=$(ls "$base"/*.son 2>/dev/null | wc -l)
    echo "notebook-baseline: snapshot OK -> $base ($n_son probe files)"
    ;;

  compare)
    [ -d "$base" ] || { echo "notebook-baseline: no snapshot for $form — run 'execute' first" >&2; exit 2; }
    mkdir -p "$base/new"
    clean_artifacts
    echo "notebook-baseline: executing $form (post-operation) ..."
    if ! run_nbconvert "$base/new" executed.ipynb; then
      echo "notebook-baseline: FAILED to execute $form (see $base/new/execute.log)" >&2
      exit 1
    fi
    echo "notebook-baseline: comparing against snapshot ..."
    # Gather the fresh probes exactly as the snapshot took them: build root
    # (extra non-scan cases) + build/coarse/ (the baseline mesh) + the
    # per-case build/<Case>/coarse/ layout of the merged MMS forms.
    mkdir -p "$base/new_sons"
    for d in "$formdir/build" "$formdir/build/coarse" "$formdir/build"/*/coarse; do
      cp "$d"/*.son "$base/new_sons/" 2>/dev/null || true
    done
    python3 "$script_dir/nb-compare.py" \
      "$base/executed.ipynb" "$base/new/executed.ipynb" \
      --son-old "$base" --son-new "$base/new_sons"
    rc=$?
    if [ $rc -eq 0 ]; then
      echo "notebook-baseline: $form VALIDATED (RULE satisfied)"
    else
      echo "notebook-baseline: $form NOT VALIDATED — see differences above" >&2
    fi
    exit $rc
    ;;
esac
