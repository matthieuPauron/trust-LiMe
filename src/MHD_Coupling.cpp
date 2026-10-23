/****************************************************************************
 * trust-LiMe — strong (Picard) coupling of the magnetic and momentum
 * equations, shared by Pb_MHD and Pb_MHD_Turbulent.
 ****************************************************************************/

#include <MHD_Coupling.h>
#include <Probleme_base.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Navier_Stokes_std.h>
#include <TRUSTTabs.h>
#include <algorithm>
#include <cmath>

namespace
{
/*! @brief Locates the magnetic and the momentum equations of a problem.
 *
 *  @param[in]  pb   the problem to scan.
 *  @param[out] eqb  the magnetic equation, or nullptr when there is none.
 *  @param[out] ns   the momentum equation, or nullptr when there is none.
 */
void locate_equations(const Probleme_base& pb,
                      const Convection_Diffusion_Magnetic_field*& eqb,
                      Navier_Stokes_std*& ns)
{
  eqb = nullptr;
  ns = nullptr;
  const int n_eq = pb.nombre_d_equations();
  for (int i = 0; i < n_eq; i++)
    {
      Equation_base& eqn = const_cast<Equation_base&>(pb.equation(i));
      if (!eqb && sub_type(Convection_Diffusion_Magnetic_field, eqn))
        eqb = &ref_cast(Convection_Diffusion_Magnetic_field, eqn);
      if (!ns && sub_type(Navier_Stokes_std, eqn))
        ns = &ref_cast(Navier_Stokes_std, eqn);
    }
}

/*! @brief Relative L2 change between two fields (0 when the reference is 0). */
double relative_change(const DoubleTab& now, const DoubleTab& before)
{
  double num = 0., den = 0.;
  const int n = std::min(now.size(), before.size());
  for (int i = 0; i < n; i++)
    {
      const double d = now(i) - before(i);
      num += d * d;
      den += before(i) * before(i);
    }
  return (den > 0.) ? std::sqrt(num / den) : 0.;
}
}

bool mhd_iterate_time_step(Probleme_base& pb, bool& converged)
{
  const Convection_Diffusion_Magnetic_field* eqb = nullptr;
  Navier_Stokes_std* ns = nullptr;
  locate_equations(pb, eqb, ns);

  // Not a coupled problem, or strong coupling not requested: plain sweep.
  if (!eqb || !ns || !eqb->couplage_fort())
    return pb.Probleme_base::iterateTimeStep(converged);

  const int maxit = eqb->nb_iterations_couplage();
  const double tol = eqb->tolerance_couplage();

  DoubleTab b_prev, u_prev;
  bool conv = false;
  for (int it = 0; it < maxit; it++)
    {
      b_prev = eqb->inconnue().valeurs();
      u_prev = ns->vitesse().valeurs();

      bool step_conv = false;
      if (!pb.Probleme_base::iterateTimeStep(step_conv))
        { converged = false; return false; }

      const double change = std::max(
                              relative_change(eqb->inconnue().valeurs(), b_prev),
                              relative_change(ns->vitesse().valeurs(), u_prev));

      if (it > 0 && change < tol)
        { conv = true; break; }
      if (it == maxit - 1)
        conv = (change < tol);
    }

  converged = conv;
  if (!conv)
    Cerr << "Picard coupling (couplage_fort) did not converge within " << maxit
         << " iterations (relative change above " << tol << ")." << finl;
  return true;
}
