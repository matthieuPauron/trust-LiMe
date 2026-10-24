# Validation targets for future VKS simulations

This file lists how the extracted experimental data can be used once a runnable
VKS case exists. The current trust-LiMe solver is low-Rm, so most high-Rm targets
are aspirational and should be treated as constraints rather than pass/fail tests.

## Inputs that can be used directly

- Sodium properties from `sodium_properties.json`
- Geometry from `geometry.json`
- Dimensionless definitions from `dimensionless_parameters.json`

## Low-Rm targets already closer to current capabilities

- Induction anisotropy and fluctuation levels from `induction_lowRm.md`
- Spectral exponents in the induction regime
- Pressure-induction correlation trends

## High-Rm targets for future work

- Dynamo threshold range Rm_c = 31-43 depending on configuration
- Saturation scaling and B ~ 40 G at 30% above threshold
- Power number increase from 0.045 to 0.065
- Regime map as a function of Theta
- Reversal durations and decay rates
- Burst amplitudes and correlation times

## Recommended comparison workflow

1. Build a VKS geometry and sodium property set from the JSON files.
2. Run a low-Rm induction case first and compare fluctuation/spectral trends.
3. Only attempt dynamo comparisons after high-Rm and turbulence capabilities exist.
4. Record simulation outputs in the same units and probe locations as the
   experimental data (P1-P5).
