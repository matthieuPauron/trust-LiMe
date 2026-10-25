@page TrustLiMe_ug_datafiles Data files

TRUST-LiMe reuses the TRUST data-file parser, so a `.data` file is a sequence of
blocks. The magnetic problem is added as an equation on top of the existing
thermohydraulics; see @ref TrustLiMe_model for the available keywords
(`Pb_MHD`, `Pb_Induction`, `Pb_MHD_Turbulent`, `Pb_MHD_Thermal`).

## Comment convention

Comments must use `# ... #` (spaces around content) or `/* ... */`. Block-based
parsing makes this consistent.

## Magnetic-specific keywords

- `induction` equation / `Pb_Induction`: standalone magnetic advection-diffusion.
- `Pb_MHD`: coupled MHD (equation 0 is always momentum).
- `force_lorentz`: Lorentz-force coupling (one- and two-way).
- `vitesse_hydraulique`: two-way velocity feedback into induction.
- `correction_divergence`: opt-in \f$\nabla\cdot b = 0\f$ projection.
- `couplage_fort`: opt-in Picard strong coupling.
- `induction_stretching`: \f$(b\cdot\nabla)u\f$ stretching source.

## General TRUST syntax

For the full keyword reference, see the **Keywords Reference Manual**
(`KeywordsReference`). For data-file structure, parallel execution rules and
validation-notebook conventions, see `AGENTS.md` and the `trust-lime-notebooks`
skill.