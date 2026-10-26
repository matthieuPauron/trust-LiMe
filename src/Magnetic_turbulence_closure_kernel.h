/****************************************************************************
 * trust-LiMe — pure arithmetic kernel of the independent magnetic k-epsilon
 * closure.  Free of TRUST framework dependencies beyond <cmath>/<algorithm>:
 * it holds the closure algebra (eta_t, source terms, production) so that it
 * can be unit-tested directly, without a mesh, solver or .data file.
 ****************************************************************************/

#ifndef MAGNETIC_TURBULENCE_CLOSURE_KERNEL_INCLUDED
#define MAGNETIC_TURBULENCE_CLOSURE_KERNEL_INCLUDED

#include <algorithm>
#include <cmath>

/*! @brief True when zero or one complete eta_t provider is selected.
 *
 *  Explicit k_b and epsilon_b are a single provider and must be supplied as a
 *  pair. Hydraulic fallback and the independent model each count as a
 *  provider in their own right.
 */
inline bool magnetic_turbulence_provider_configuration_valid(bool explicit_eta_t,
                                                             bool explicit_k,
                                                             bool explicit_epsilon,
                                                             bool hydraulic_fallback,
                                                             bool independent_model)
{
  if (explicit_k != explicit_epsilon) return false;
  const int providers = static_cast<int>(explicit_eta_t) + static_cast<int>(explicit_k)
                        + static_cast<int>(hydraulic_fallback)
                        + static_cast<int>(independent_model);
  return providers <= 1;
}

/*! @brief Turbulent magnetic diffusivity
 *         \f$\eta_t = \min(C_{\mu,b}\,k_b^2/(\max(\varepsilon_b,\varepsilon_{\min})\,Pm_{t,b}),\;\eta_{t,\max})\f$.
 *
 *  Negative \f$k_b\f$ is clamped to 0 (so \f$\eta_t=0\f$).  Mirrors
 *  Champ_Fonc_Eta_Turbulent::calculer() for the non-realizable branch.
 */
inline double magnetic_turbulence_eta_t(double k, double eps, double C_mu, double Pm_t,
                                        double eps_floor, double eta_t_cap)
{
  double kk = (k < 0.) ? 0. : k;
  double ee = (eps < eps_floor) ? eps_floor : eps;
  if (ee <= 0.) ee = eps_floor; // guard
  if (Pm_t <= 0.) return 0.;
  double val = C_mu * kk * kk / (ee * Pm_t);
  if (val > eta_t_cap) val = eta_t_cap;
  if (!std::isfinite(val) || val < 0.) val = 0.;
  return val;
}

/*! @brief k_b source term: \f$P_b - \varepsilon_b\f$. */
inline double magnetic_turbulence_k_source(double production, double eps)
{
  return production - eps;
}

/*! @brief epsilon_b source term:
 *         \f$C_{\varepsilon1,b}(\varepsilon_b/k_b)P_b - C_{\varepsilon2,b}\varepsilon_b^2/k_b\f$.
 *  \f$k_b\f$ and \f$\varepsilon_b\f$ are floored to k_floor / eps_floor. */
inline double magnetic_turbulence_epsilon_source(double C_eps1, double C_eps2,
                                                 double eps, double k,
                                                 double eps_floor, double k_floor,
                                                 double production)
{
  double kb = (k < k_floor) ? k_floor : k;
  if (kb <= 0.) kb = k_floor;
  double eb = (eps < eps_floor) ? eps_floor : eps;
  if (eb <= 0.) eb = eps_floor;
  return C_eps1 * (eb / kb) * production - C_eps2 * eb * eb / kb;
}

/*! @brief Production term \f$P_b = \eta_t\,\Phi^2\,|\nabla\times b_A|^2\f$,
 *         where \f$\Phi = 1/\sqrt{\mu_0 \rho_{\rm ref}}\f$ is field_scale.
 *  For a uniform field curl=0 -> 0. */
inline double magnetic_turbulence_production(double eta_t, double field_scale, double curl2)
{
  return eta_t * field_scale * field_scale * curl2;
}

#endif /* MAGNETIC_TURBULENCE_CLOSURE_KERNEL_INCLUDED */
