---
description: Build and run a TRUST validation notebook end to end
---
Build the module, then execute the validation notebook $ARGUMENTS (default: `share/Validation/Rapports_automatiques/HartmannFlow/HartmannFlow.ipynb`).

The notebooks must run inside the TRUST Python environment (venv_trust plus the TRUST env), NOT the OpenCode MCP venv. Run everything in one shell invocation:

    source ../trust-code/env_TRUST.sh
    source ../trust-code/env_for_python.sh
    source ~/Development/venv_trust/bin/activate
    make module_semi_optim
    jupyter nbconvert --to notebook --execute \
      --output-dir /tmp/opencode/trust-lime-notebooks \
      "<notebook>"

Notebooks:
- share/Validation/Rapports_automatiques/MagneticStandalone/MagneticStandalone.ipynb (standalone magnetic solver; self-contained datasets + analytic/invariant assertions)
- share/Validation/Rapports_automatiques/HartmannFlow/HartmannFlow.ipynb
- share/Validation/Rapports_automatiques/Shercliff_3D_duct/Shercliff_3D_duct.ipynb

The first one is the only case that matches what the module currently solves; the
other two are placeholders for the future coupled MHD problem.

The notebooks drive `trustutils.run` (`addCase` / `runCases` / `tablePerf`) and read
results from `run.BUILD_DIRECTORY/<dataset name>/`. Use a generous timeout. Summarize
the outputs; if a cell errors, show its traceback and stop. Do not modify the notebook
unless I ask.

Load the `trust-lime` skill for the full recipe.
