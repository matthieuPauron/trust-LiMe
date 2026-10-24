# VKS experimental reference data

This folder collects measurements and analysis extracted from the von Kármán
Sodium (VKS) experimental papers listed in `sources.json`. It is intended to
support future VKS validation work in trust-LiMe; it is **not** a runnable
simulation case.

## Files

| File | Content |
|---|---|
| `sources.json` | Bibliographic source inventory |
| `geometry.json` / `geometry.md` | Experimental geometry and probe locations |
| `sodium_properties.json` / `sodium_properties.md` | Sodium properties and operating ranges |
| `dimensionless_parameters.json` / `dimensionless_parameters.md` | Dimensionless groups and typical ranges |
| `induction_lowRm.json` / `induction_lowRm.md` | Low-Rm induction measurements |
| `dynamo_threshold_saturation.json` / `dynamo_threshold_saturation.md` | Dynamo threshold, saturation, and power |
| `fluctuations_spectra.json` / `fluctuations_spectra.md` | Fluctuations and spectral observations |
| `dynamical_regimes.json` / `dynamical_regimes.md` | Regime map, reversals, bursts, oscillations |
| `validation_targets.md` | How these data can be used by future simulations |

## Units and conventions

- All physical quantities are given in SI units unless explicitly stated.
- Magnetic field values are given in gauss (G) when quoting experimental plots;
  convert to tesla (1 G = 1e-4 T) before comparing with simulation output.
- Source references point to the paper ID in `sources.json` and, where possible,
  to a section, equation, or figure.

## Important caveat

The current trust-LiMe implementation is a low-Rm solver. The VKS experiment
operates at Rm ~ 10-50 and is strongly turbulent. These data are therefore
reference targets and physical constraints for future work, not assertions that
the current code is expected to reproduce.
