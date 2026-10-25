@page TrustLiMe_tut_firstcase Running a first case

A minimal path to running an existing validation form end-to-end.

1. Source the environment: `source env_trust_LiMe.sh`.
2. Build the module: `make module_semi_optim`.
3. Pick a form under `share/Validation/Rapports_automatiques/` (e.g. a
   `MagneticStandalone` case).
4. Run the notebook or the `.data` case per the form's instructions.
5. Inspect outputs and post-treatment fields (`Champs2D` / `Champs3D`).

For writing a new validation form, see the `trust-lime-notebooks` skill and
`share/Validation/Rapports_automatiques/`. For the data-file syntax, see
@ref TrustLiMe_ug_datafiles "Data files".