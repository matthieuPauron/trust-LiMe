#!/usr/bin/env bash
# run-all MagneticStandalone validation notebooks, export each to PDF, and
# concatenate the PDFs into a single report at the trust-LiMe repository root.
repo=/home/matthieu/Development/trust-LiMe
# 2D first, then 3D: forms are visited in category order.
cats="MagneticStandalone MagneticStandalone3D"
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
for cat in $cats; do
  nbroot=$repo/share/Validation/Rapports_automatiques/$cat
  for nb in "$nbroot"/*/*.ipynb; do
    [ -f "$nb" ] || continue
    form=$(basename "$(dirname "$nb")")
    # unique output name: form names repeat across categories (e.g. Invariance)
    tag="${cat}_${form}"
    log "=== $cat/$form"
    jupyter nbconvert --to notebook --execute \
      --output-dir "$outdir" --output "$tag" "$nb" >> "$runlog" 2>&1
    r=$?
    if [ $r -ne 0 ]; then
      log "FAILED (execute): $cat/$form"
      rc_total=1
      continue
    fi
    jupyter nbconvert --to pdf --output-dir "$pdfdir" --output "$tag" \
      "$outdir/${tag}.ipynb" >> "$runlog" 2>&1
    r=$?
    if [ $r -ne 0 ]; then
      log "FAILED (pdf): $cat/$form"
      rc_total=1
      continue
    fi
    pdfs+=("$pdfdir/${tag}.pdf")
    log "OK: $cat/$form -> $pdfdir/${tag}.pdf"
  done
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