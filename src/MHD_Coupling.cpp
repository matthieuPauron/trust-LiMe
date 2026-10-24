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

  // Save the state at the beginning of the time step (t = t_old).
  DoubleTab b_old = eqb->inconnue().valeurs();
  DoubleTab u_old = ns->vitesse().valeurs();

  // Enable flags so faire_un_pas_de_temps_eqn_base does not overwrite
  // passe (time derivative) or re-impose boundary conditions.
  pb.schema_temps().set_skip_passe_update(true);
  pb.schema_temps().set_skip_cond_lim(true);

  DoubleTab b_prev = b_old;
  DoubleTab u_prev = u_old;
  bool conv = false;
  for (int it = 0; it < maxit; it++)
    {
      // Restore passe to the old (t-1) values for correct time derivative.
      const_cast<DoubleTab&>(eqb->inconnue().passe()) = b_old;
      const_cast<DoubleTab&>(ns->vitesse().passe()) = u_old;

      // Use the previous sweep's result as the initial guess (present).
      const_cast<DoubleTab&>(eqb->inconnue().valeurs()) = b_prev;
      const_cast<DoubleTab&>(ns->vitesse().valeurs()) = u_prev;

      // Also set futur = previous sweep's result (lagged value for sources).
      const_cast<DoubleTab&>(eqb->inconnue().futur()) = b_prev;
      const_cast<DoubleTab&>(ns->vitesse().futur()) = u_prev;

      bool step_conv = false;
      if (!pb.Probleme_base::iterateTimeStep(step_conv))
        { converged = false; return false; }

      // After iterateTimeStep: futur = new solution, present = old (restored
      // by faire_un_pas_de_temps_eqn_base), passe = old (preserved by flag).

      // Clean the new solution's divergence (Gauss' law) when the correction
      // is enabled: the new solution lives in futur(), not valeurs().
      {
        Convection_Diffusion_Magnetic_field* eqb_nc = const_cast<Convection_Diffusion_Magnetic_field*>(eqb);
        DoubleTab& fut_b_nc = const_cast<DoubleTab&>(eqb->inconnue().futur());
        eqb_nc->corriger_divergence(fut_b_nc);
        fut_b_nc.echange_espace_virtuel();
      }

      // Read the new solution from futur (dimensions_ may be -1, use addr).
      const DoubleTab& fut_b = eqb->inconnue().futur();
      const DoubleTab& fut_u = ns->vitesse().futur();

      // Compute relative change.
      double num_b = 0., den_b = 0.;
      const int n_b = std::min((int)fut_b.size_array(), (int)b_prev.size_array());
      for (int i = 0; i < n_b; i++)
        {
          const double d = fut_b.addr()[i] - b_prev.addr()[i];
          num_b += d * d;
          den_b += b_prev.addr()[i] * b_prev.addr()[i];
        }
      double num_u = 0., den_u = 0.;
      const int n_u = std::min((int)fut_u.size_array(), (int)u_prev.size_array());
      for (int i = 0; i < n_u; i++)
        {
          const double d = fut_u.addr()[i] - u_prev.addr()[i];
          num_u += d * d;
          den_u += u_prev.addr()[i] * u_prev.addr()[i];
        }
      const double change_b = (den_b > 0.) ? std::sqrt(num_b / den_b) : 0.;
      const double change_u = (den_u > 0.) ? std::sqrt(num_u / den_u) : 0.;
      const double change = std::max(change_b, change_u);

      Cerr << "Picard sweep " << (it + 1) << "/" << maxit
           << ": relative change = " << change << finl;

      if (it > 0 && change < tol)
        { conv = true; break; }
      if (it == maxit - 1)
        conv = (change < tol);

      // Save futur values into b_prev/u_prev for next iteration.
      for (int i = 0; i < (int)fut_b.size_array(); i++)
        b_prev.addr()[i] = fut_b.addr()[i];
      for (int i = 0; i < (int)fut_u.size_array(); i++)
        u_prev.addr()[i] = fut_u.addr()[i];
    }

  // Disable flags.
  pb.schema_temps().set_skip_passe_update(false);
  pb.schema_temps().set_skip_cond_lim(false);

  // Restore passe = old (final).
  const_cast<DoubleTab&>(eqb->inconnue().passe()) = b_old;
  const_cast<DoubleTab&>(ns->vitesse().passe()) = u_old;

  // Do NOT call validateTimeStep here — the caller (solveTimeStep/main loop)
  // handles time advancement and media/BC updates.

  converged = conv;
  if (!conv)
    Cerr << "Picard coupling (couplage_fort) did not converge within " << maxit
         << " iterations (relative change above " << tol << ")." << finl;
  return true;
}
