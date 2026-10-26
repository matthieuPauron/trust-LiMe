---
name: trust-LiMe validation report
description: >
  Generate the consolidated validation report: execute notebooks, export per-form
  PDFs, concatenate with pdfunite. Use when refreshing trust-lime_validation.pdf
  or after a full build. Never commits the generated PDFs.
---

# trust-LiMe validation report

## Full pipeline

```bash
.opencode/scripts/run-validation-pdf.sh   # from the repo root, TRUST env sourced
```

The runner visits the four categories in order
(`MagneticStandalone`, `MagneticStandalone3D`, `MagneticCoupled`,
`MagneticCoupled3D`... plus `Postraitement_fields`) and names each per-form PDF
`<Category>_<FormName>.pdf`.

## Manual steps (when the runner is not enough)

```bash
# execute
jupyter nbconvert --to notebook --execute \
  --output-dir /tmp/opencode/lime_pdf/notebooks Form/Form.ipynb
# export to PDF
jupyter nbconvert --to pdf --output-dir /tmp/opencode/lime_pdf/pdf executed.ipynb
# concatenate
pdfunite /tmp/opencode/lime_pdf/pdf/*.pdf trust-lime_validation.pdf
```

## Rules

- Inline math in a table cell must close without a space before `$`
  (`$\nabla\times b$`, never `$\nabla\times b $`) — pandoc mis-parses the
  trailing space and XeTeX aborts.
- Embed images as markdown `![](src/fig.png)`, not HTML, for PDF export.
- Negative (diagnostic) forms end with a summary print instead of
  `run.tablePerf()` — they run `trust` directly and have no suite.
- The concatenated report is named `trust-lime_validation.pdf` at the repo
  root and is a generated artifact: **never commit it** (like `build/`).

## Pitfalls

- Run from the repo root with the TRUST Python env sourced (`venv_trust` +
  `env_trust_LiMe.sh`); `$exec` must point at the trust-LiMe module.
- Clean stale `.stop`/`.out`/`.err`/`*.son`/`*.TU` before re-running.
- A new category must be added to the runner's `cats` list.
