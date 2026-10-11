---
description: Build and run a TRUST validation notebook end to end
---
Load the `trust-lime-notebooks` skill for the full recipe (env, hard rules,
sequential-only constraint).

Build the module, then execute the validation notebook $ARGUMENTS (default:
the MagneticStandalone one, see below), headlessly via `jupyter nbconvert`.

The notebooks must run inside the TRUST Python environment (venv_trust plus the
TRUST env), NOT the OpenCode MCP venv. Run everything in one shell invocation:

    source ../trust-code/env_TRUST.sh
    source ../trust-code/env_for_python.sh
    source ~/Development/venv_trust/bin/activate
    source env_trust_LiMe.sh        # sets $exec to the MODULE binary (trust-lime_opt)
    make module_semi_optim
    # Guard: trustutils runs the binary named in $exec. It MUST be trust-lime,
    # otherwise every case fails with "PB_MHD is not a recognized keyword".
    [[ "$exec" == *trust-lime* ]] || { echo "WRONG exec: $exec" >&2; exit 1; }
    jupyter nbconvert --to notebook --execute \
      --output-dir /tmp/opencode/trust-lime-notebooks \
      "<notebook>"

Notebooks (under share/Validation/Rapports_automatiques/):
- MagneticStandalone/MagneticStandalone.ipynb

HartmannFlow and Shercliff_3D_duct were removed; recreate them only when the
coupled MHD problem lands. A notebook must contain tests of **one single ASN28
category**; see `docs/content/test_cases.md` (taxonomy + per-case table) and
the `trust-lime-testing` skill.

Use a generous timeout. Summarize the outputs; if a cell errors, show its
traceback and stop. Do not modify the notebook unless I ask.
