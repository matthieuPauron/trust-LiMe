---
description: Build and run a TRUST validation notebook end to end
---
Load the `trust-lime-notebooks` skill for the full recipe (env, hard rules,
sequential-only constraint).

First run the formatting gate — abort if it fails:

    .opencode/scripts/check-validation.sh || exit 1

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

Notebooks (under share/Validation/Rapports_automatiques/): one
`Rapports_automatiques/<Category>/<FormName>/<FormName>.ipynb` per test case.
The standalone magnetic forms are `MagneticStandalone/*/`:
`Invariance/`, `Diffusion/`, `DiffusionRefined/`, `Advection/`,
`SteadyState/`, `VaryingVelocity/`, `SourceStretching/`, and the
negative `Error*` forms.

HartmannFlow and Shercliff_3D_duct were removed; recreate them only when the
coupled MHD problem lands. A notebook runs **one and only one test case** and
contains tests of **one single ASN28 category**; see `docs/content/test_cases.md`
(taxonomy + per-case table) and the `trust-lime-testing` skill.

Use a generous timeout. Summarize the outputs; if a cell errors, show its
traceback and stop. Do not modify the notebook unless I ask.
