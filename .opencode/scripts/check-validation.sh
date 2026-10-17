#!/usr/bin/env bash
# check-validation.sh — enforce the trust-LiMe validation-case formatting rules.
#
# Canonical rule text lives in .opencode/skills/trust-lime-notebooks/SKILL.md
# (and is summarised in AGENTS.md and docs/content/test_cases.md).
#
#   R1  notebook placement
#       Every .ipynb lives at the form root,
#         share/Validation/Rapports_automatiques/<Category>/<FormName>/<FormName>.ipynb
#       (or <Case>/<Case>.ipynb when the case folder is itself the form),
#       never in src/ (nor in any deeper directory).
#
#   R2  sequential-only datasets (default)
#       No .data may contain TRUST parallelism machinery:
#       Decouper, Partitionneur, Partition, Partition_tool, Scatter,
#       Larg_joint, Nom_Zones, zones_name — this also covers the
#       "# BEGIN PARTITION #" / "# BEGIN SCATTER #" section markers.
#       Exception: paths listed in parallel_ok[] below.
#
#   R3  sequential-only notebooks (default)
#       No case.partition(), no nbProcs > 1, no `trust -partition` CLI flag.
#       Exception: paths listed in parallel_ok[] below.
#
#   R4  one notebook, one test case
#       A notebook runs one and only one test case: it must not issue more
#       than one run.addCase()/addCaseFromTemplate() call. Shared helpers may
#       be duplicated, but a notebook never loops over several datasets.
#
# Usage:
#   check-validation.sh [--all] [--quiet]
#
#     --all     Scan the working tree instead of only git-tracked files.
#               By default the generated, git-ignored build/ and
#               .ipynb_checkpoints/ trees are skipped.
#     --quiet   Print only the violations, not the summary line.
#
# Exit status: 0 = clean, 1 = violations found, 2 = usage/environment error.
set -uo pipefail

usage() {
  cat <<'EOF'
check-validation.sh — enforce validation-case formatting rules (R1-R4).

Usage: check-validation.sh [--all] [--quiet]
  --all     scan the working tree instead of only git-tracked files
  --quiet   print only the violations, not the summary line

Rules:
  R1  every .ipynb lives at the form root (…/<Category>/<FormName>/<FormName>.ipynb,
      or …/<Case>/<Case>.ipynb), never in src/ or deeper
  R2  no .data contains TRUST parallelism machinery (Decouper, Partitionneur,
      Partition, Partition_tool, Scatter, Larg_joint, Nom_Zones, zones_name)
      — except paths listed in parallel_ok[] (see SKILL.md)
  R3  no notebook calls partition(), uses nbProcs>1 or `trust -partition`
      — except paths listed in parallel_ok[] (see SKILL.md)
  R4  a notebook runs one and only one test case (at most one addCase call)

Exit status: 0 clean, 1 violations, 2 usage error.
EOF
}

scan_all=0
quiet=0
for arg in "$@"; do
  case "$arg" in
    --all)     scan_all=1 ;;
    --quiet)   quiet=1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "check-validation: unknown option: $arg" >&2; usage >&2; exit 2 ;;
  esac
done

# Resolve the repository root so the script works from any cwd.
if ! repo_root=$(git rev-parse --show-toplevel 2>/dev/null); then
  repo_root=$(cd "$(dirname "$0")/../.." && pwd)
fi
cd "$repo_root" || exit 2

base=share/Validation/Rapports_automatiques
if [ ! -d "$base" ]; then
  echo "check-validation: no $base directory (run from the trust-LiMe checkout)" >&2
  exit 2
fi

# Cases explicitly allowed to run on several processors (R2/R3 exception).
# Every proper validation form (2-level layout) is ported to 2 ranks; add a new
# form here when it is ported — never disable R2/R3 globally. Each entry is a
# directory prefix under $base, with a trailing slash.
parallel_ok=(
  # --- MagneticStandalone (2D) ---
  "$base/MagneticStandalone/Advection/"
  "$base/MagneticStandalone/Diffusion/"
  "$base/MagneticStandalone/DiffusionRefined/"
  "$base/MagneticStandalone/ErrorBadBc/"
  "$base/MagneticStandalone/ErrorBadNbcomposantes/"
  "$base/MagneticStandalone/ErrorMissingEta/"
  "$base/MagneticStandalone/Hartmann1D/"
  "$base/MagneticStandalone/Invariance/"
  "$base/MagneticStandalone/MMS_Diff2D/"
  "$base/MagneticStandalone/MMS_ExpTrig2D/"
  "$base/MagneticStandalone/MMS_Poly_2D/"
  "$base/MagneticStandalone/MMS_Trig2D/"
  "$base/MagneticStandalone/Shercliff2D/"
  "$base/MagneticStandalone/SourceStretching/"
  "$base/MagneticStandalone/SteadyState/"
  "$base/MagneticStandalone/VaryingVelocity/"
  # --- MagneticStandalone3D (3D twins) ---
  "$base/MagneticStandalone3D/Advection/"
  "$base/MagneticStandalone3D/Diffusion/"
  "$base/MagneticStandalone3D/DiffusionRefined/"
  "$base/MagneticStandalone3D/ErrorBadBc/"
  "$base/MagneticStandalone3D/ErrorBadNbcomposantes/"
  "$base/MagneticStandalone3D/ErrorMissingEta/"
  "$base/MagneticStandalone3D/Hartmann1D/"
  "$base/MagneticStandalone3D/Invariance/"
  "$base/MagneticStandalone3D/MMS_Diff3D/"
  "$base/MagneticStandalone3D/MMS_ExpTrig3D/"
  "$base/MagneticStandalone3D/MMS_Poly_3D/"
  "$base/MagneticStandalone3D/MMS_Trig3D/"
  "$base/MagneticStandalone3D/SourceStretching/"
  "$base/MagneticStandalone3D/SteadyState/"
  "$base/MagneticStandalone3D/VaryingVelocity/"
  # --- MagneticCoupled ---
  "$base/MagneticCoupled/ForceLorentzBalance/"
  # --- Postraitement_fields ---
  "$base/Postraitement_fields/CourantMagnetique/"
  "$base/Postraitement_fields/DivergenceMagnetique/"
  "$base/Postraitement_fields/EnergieMagnetique/"
  "$base/Postraitement_fields/ForceLorentz/"
  "$base/Postraitement_fields/HeliciteCinetique/"
  "$base/Postraitement_fields/HeliciteCroisee/"
  "$base/Postraitement_fields/NombreDePrandtlMagnetique/"
  "$base/Postraitement_fields/NombreDeReynoldsMagnetique/"
  "$base/Postraitement_fields/PulsationCyclotronique/"
  "$base/Postraitement_fields/RotationnelMagnetique/"
  "$base/Postraitement_fields/TensionMagnetique/"
  "$base/Postraitement_fields/VecteurPoynting/"
)

is_parallel_ok() {
  local f="$1" p
  for p in "${parallel_ok[@]}"; do
    case "$f" in
      "$p"*) return 0 ;;
    esac
  done
  return 1
}

files=()
if [ "$scan_all" -eq 1 ]; then
  while IFS= read -r -d '' f; do
    files+=("$f")
  done < <(
    find "$base" -type f \( -name '*.data' -o -name '*.ipynb' \) \
      -not -path '*/.ipynb_checkpoints/*' \
      -not -path '*/build/*' \
      -not -name '*.swp' -print0
  )
else
  while IFS= read -r f; do
    case "$f" in
      *.data|*.ipynb) files+=("$f") ;;
    esac
  done < <(git ls-files -- "$base")
fi

violations=0
report() {
  violations=$((violations + 1))
  printf '%s:%s: %s\n' "$1" "$2" "$3"
}

# --- R1: notebook placement --------------------------------------------------
for f in "${files[@]}"; do
  [ -f "$f" ] || continue   # tracked but deleted in the working tree
  case "$f" in
    *.ipynb)
      dir=$(dirname "$f")
      # Allowed: <base>/<Category>/<FormName>/<FormName>.ipynb — i.e. at most
      # two path components below base (a case root and a form root). Anything
      # deeper (src/, build/, .ipynb_checkpoints/) is a violation.
      rel="${dir#"$base"/}"
      case "$rel" in
        */*/*)
          report "$f" "-" \
            "R1 notebook must live at the form root ($base/<Category>/<FormName>/<FormName>.ipynb), not under ${dir#"$base"/}/"
          ;;
      esac
      ;;
  esac
done

# --- R2 / R3 / R4: sequential-only and one-case-per-notebook checks ----------
kw_re='\b(Decouper|Partitionneur|Partition|Partition_tool|Scatter|Larg_joint|Nom_Zones|zones_name)\b'
nb_re='(\.partition[[:space:]]*\(|nbProcs[[:space:]]*=[[:space:]]*([2-9]|[1-9][0-9])|-partition)'
addcase_re='(\.addCase[[:space:]]*\(|\.addCaseFromTemplate[[:space:]]*\()'

for f in "${files[@]}"; do
  [ -f "$f" ] || continue   # tracked but deleted in the working tree
  case "$f" in
    *.data)
      if is_parallel_ok "$f"; then
        # Allowlisted parallel case: the three marker regions must be present
        # so that `trust -partition` can build the PAR_ variant.
        for marker in '# BEGIN MESH #' '# BEGIN PARTITION' '# BEGIN SCATTER'; do
          grep -qF -- "$marker" "$f" || report "$f" "-" \
            "R2 allowlisted parallel dataset missing '$marker' marker"
        done
        continue
      fi
      while IFS=: read -r line _; do
        [ -n "${line:-}" ] || continue
        report "$f" "$line" "R2 forbidden parallelism keyword (datasets must be sequential-only)"
      done < <(grep -nEi -- "$kw_re" "$f" || true)
      ;;
    *.ipynb)
      if ! is_parallel_ok "$f"; then
        while IFS=: read -r line _; do
          [ -n "${line:-}" ] || continue
          report "$f" "$line" "R3 forbidden parallelism in notebook (must run sequentially)"
        done < <(grep -nEi -- "$nb_re" "$f" || true)
      fi
      ncases=$(grep -Eo -- "$addcase_re" "$f" | wc -l || true)
      if [ "${ncases:-0}" -gt 1 ]; then
        report "$f" "-" \
          "R4 notebook runs $ncases test cases (one and only one addCase per notebook)"
      fi
      ;;
  esac
done

# --- summary -----------------------------------------------------------------
if [ "$quiet" -eq 0 ]; then
  printf '\ncheck-validation: %d file(s) scanned, %d violation(s)\n' "${#files[@]}" "$violations"
fi

if [ "$violations" -ne 0 ]; then
  cat <<'EOF'
Fix: datasets must be sequential — strip Decouper/Scatter and every partition
keyword; notebooks must live at the case root, never call partition() or pass
-partition, and must run one and only one test case (one addCase per notebook).
The only exception is a path listed in parallel_ok[] (see the trust-lime-notebooks
skill) — add it there deliberately, never by disabling R2/R3.
EOF
  exit 1
fi

echo "check-validation: OK"
exit 0
