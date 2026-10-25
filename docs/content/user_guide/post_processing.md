@page TrustLiMe_ug_postproc Post-processing

Post-treatment fields are provided by `Champ_Fonc_MHD` (merged into
`Champs2D` / `Champs3D` forms on 2026-10-21). They expose derived magnetic
quantities for visualization and analysis.

## Available fields

See @ref TrustLiMe_api "C++ API reference" for `Champ_Fonc_MHD` and related
classes. Common derived quantities include the Lorentz force, current density
and magnetic energy.

## Validation forms

The `Champs2D` / `Champs3D` validation forms exercise the post-treatment
fields end-to-end; see @ref TrustLiMe_validation "Validation cases".