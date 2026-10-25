@page TrustLiMe_userguide User Guide

This section collects the practical information needed to **run** TRUST-LiMe:
data-file syntax, execution, and post-processing. The equations and numerics live
in @ref TrustLiMe_model "Problems, equations and boundary conditions".

- @subpage TrustLiMe_ug_datafiles
- @subpage TrustLiMe_ug_postproc

## Data files

TRUST data files are block-based. Comments must use `# ... #` (spaces around
content) or `/* ... */`. See @ref TrustLiMe_ug_datafiles "Data files".

## Post-processing

Post-treatment fields (`Champ_Fonc_MHD`) and their forms are merged into
`Champs2D` / `Champs3D`. See @ref TrustLiMe_ug_postproc "Post-processing".