#!/usr/bin/env bash
# run-all MagneticStandalone validation notebooks, export each to PDF, and
# concatenate the PDFs into a single report at the trust-LiMe repository root.
repo=/home/matthieu/Development/trust-LiMe
nbroot=$repo/share/Validation/Rapports_automatiques/MagneticStandalone
work=/tmp/opencode/lime_pdf
outdir=$work/notebooks
pdfdir=$work/pdf
mkdir -p "$outdir" "$pdfdir"
runlog="$work/run.log"
: > "$runlog"
log() { printf '%s\n' "$*" | tee -a "$runlog"; }

cd "$repo" || exit 1

# --- TRUST python environment + module binary --------------------------------
source ../trust-code/env_TRUST.sh
source ../trust-code/env_for_python.sh
source "$repo/env_trust_LiMe.sh"
source ~/Development/venv_trust/bin/activate

if [ -n "${exec:-}" ] && [[ "$exec" == *trust-lime* ]]; then
  log "exec OK: $exec"
else
  log "WRONG exec: ${exec:-<unset>}"
  exit 1
fi

shell_name=$(basename "$SHELL")
log "shell=$shell_name"

rc_total=0
pdfs=()
for nb in "$nbroot"/*/*.ipynb; do
  [ -f "$nb" ] || continue
  form=$(basename "$(dirname "$nb")")
  log "=== $form"
  jupyter nbconvert --to notebook --execute \
    --output-dir "$outdir" "$nb" >> "$runlog" 2>&1
  r=$?
  if [ $r -ne 0 ]; then
    log "FAILED (execute): $form"
    rc_total=1
    continue
  fi
  done_nb="$outdir/$(basename "$nb")"
  jupyter nbconvert --to pdf --output-dir "$pdfdir" "$done_nb" >> "$runlog" 2>&1
  r=$?
  if [ $r -ne 0 ]; then
    log "FAILED (pdf): $form"
    rc_total=1
    continue
  fi
  pdf="$pdfdir/${form}.pdf"
  produced="$pdfdir/$(basename "$nb" .ipynb).pdf"
  if [ "$produced" != "$pdf" ]; then mv "$produced" "$pdf"; fi
  pdfs+=("$pdf")
  log "OK: $form -> $pdf"
done

log "--- concatenating ${#pdfs[@]} PDFs"
if [ "${#pdfs[@]}" -gt 0 ]; then
  pdfunite "${pdfs[@]}" "$repo/MagneticStandalone_validation.pdf" 2>>"$runlog"
  r=$?
  if [ $r -eq 0 ]; then
    log "joined -> $repo/MagneticStandalone_validation.pdf"
    ls -la "$repo/MagneticStandalone_validation.pdf" >> "$runlog"
  else
    log "FAILED (pdfunite rc=$r)"
    rc_total=1
  fi
fi

log "run-all rc=$rc_total  (log: $runlog)"
exit $rc_total