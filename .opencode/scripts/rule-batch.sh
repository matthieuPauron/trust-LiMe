#!/usr/bin/env bash
# rule-batch.sh — RULE-gate the nb-scan-rename conversion over a list of forms.
# Per form: execute (snapshot) -> transform -> compare. 3 forms in parallel.
repo=/home/matthieu/Development/trust-LiMe
log=/tmp/opencode/rule_batch.log
: > "$log"
cd "$repo" || exit 1

forms=(
  MagneticStandalone/Advection
  MagneticStandalone/Diffusion
  MagneticStandalone/Hartmann1D
  MagneticStandalone/Invariance
  MagneticStandalone/Shercliff2D
  MagneticStandalone/SourceStretching
  MagneticStandalone/SteadyState
  MagneticStandalone/VaryingVelocity
  MagneticCoupled/ForceLorentzBalance
  MagneticCoupled/MMS_Coupled
  Postraitement_fields/Champs2D
  Postraitement_fields/Champs3D
)

one_form() {
  f="$1"
  tag=${f//\//_}
  ok=0
  if bash .opencode/scripts/notebook-baseline.sh execute "$f" > "/tmp/opencode/rule_${tag}.execute.log" 2>&1; then
    if python3 .opencode/scripts/nb-scan-rename.py "$f" >> "$log" 2>&1; then
      if bash .opencode/scripts/notebook-baseline.sh compare "$f" > "/tmp/opencode/rule_${tag}.compare.log" 2>&1; then
        ok=1
      else
        echo "COMPARE-FAIL $f" >> "$log"
      fi
    else
      echo "TRANSFORM-FAIL $f" >> "$log"
    fi
  else
    echo "EXECUTE-FAIL $f" >> "$log"
  fi
  if [ "$ok" = 1 ]; then
    echo "VALIDATED $f" >> "$log"
  fi
}
export -f one_form
export log

printf '%s\n' "${forms[@]}" | xargs -P 3 -I{} bash -c 'one_form "$@"' _ {}

echo "=== batch done ===" >> "$log"
grep -cE "^(VALIDATED|EXECUTE-FAIL|TRANSFORM-FAIL|COMPARE-FAIL)" "$log" | tee -a "$log"
grep -E "FAIL" "$log" || echo "no failures" | tee -a "$log"
