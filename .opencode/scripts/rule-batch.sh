#!/usr/bin/env bash
# rule-batch.sh — RULE-gate the nb-scan-rename conversion over a list of forms.
# Per form: execute (snapshot) -> transform -> compare. 3 forms in parallel.
repo=/home/matthieu/Development/trust-LiMe
log=/tmp/opencode/rule_batch.log
: > "$log"
cd "$repo" || exit 1

forms=(
  MagneticStandalone/Advection
  MagneticStandalone/Hartmann1D
  MagneticStandalone/Invariance
  MagneticStandalone/Shercliff2D
  MagneticStandalone/SourceStretching
  MagneticStandalone/SteadyState
  MagneticStandalone/VaryingVelocity
  MagneticStandalone3D/Advection
  MagneticStandalone3D/Diffusion
  MagneticStandalone3D/Hartmann1D
  MagneticStandalone3D/Invariance
  MagneticStandalone3D/SourceStretching
  MagneticStandalone3D/SteadyState
  MagneticStandalone3D/VaryingVelocity
  MagneticCoupled/ForceLorentzBalance
  MagneticCoupled/MMS_Coupled
  MagneticCoupled3D/MMS_Coupled3D
  Postraitement_fields/CourantMagnetique
  Postraitement_fields/DivergenceMagnetique
  Postraitement_fields/EnergieMagnetique
  Postraitement_fields/ForceLorentz
  Postraitement_fields/HeliciteCinetique
  Postraitement_fields/HeliciteCroisee
  Postraitement_fields/NombreDePrandtlMagnetique
  Postraitement_fields/NombreDeReynoldsMagnetique
  Postraitement_fields/PulsationCyclotronique
  Postraitement_fields/RotationnelMagnetique
  Postraitement_fields/TensionMagnetique
  Postraitement_fields/VecteurPoynting
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
