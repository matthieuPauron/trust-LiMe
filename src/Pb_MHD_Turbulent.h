/****************************************************************************
 * trust-LiMe — Pb_MHD_Turbulent: hydro + magnetic induction, turbulent
 * momentum equation.
 *
 * Same layout as Pb_MHD, with the momentum equation solved by
 * Navier_Stokes_Turbulent (which carries its own k-eps transport equation):
 *
 *     Pb_MHD_Turbulent pb
 *     {
 *         nb_composantes 2
 *         fluide_incompressible { ... }
 *         Navier_Stokes_turbulent
 *         {
 *             modele_turbulence K_Epsilon { ... }   # provides k and eps
 *             sources { force_lorentz { } }         # b -> u
 *         }
 *         Convection_Diffusion_Magnetic_field
 *         {
 *             vitesse_hydraulique                   # u -> b, eta_t from k, eps
 *             ...
 *         }
 *     }
 *
 * The momentum equation must stay at index 0: the hydraulic turbulence model
 * is read through the normal Probleme_base path, which assumes it (see
 * Probleme_base::typer_lire_milieu, "ns_ou_cond_eq = 0").
 *
 *  @sa Pb_Hydraulique_Turbulent, Pb_MHD, Pb_Induction,
 *      Convection_Diffusion_Magnetic_field
 ****************************************************************************/

#ifndef PB_MHD_TURBULENT_H_
#define PB_MHD_TURBULENT_H_

#include <Pb_Hydraulique_Turbulent.h>
#include <Convection_Diffusion_Magnetic_field.h>

/*! @brief Coupled hydro + magnetic induction problem, turbulent momentum
 *         equation with a k-eps model.
 *
 *  @sa Pb_Hydraulique_Turbulent, Pb_MHD, Pb_Induction
 */
class Pb_MHD_Turbulent : public Pb_Hydraulique_Turbulent
{
  Declare_instanciable(Pb_MHD_Turbulent);

public:

  int nombre_d_equations() const override;
  const Equation_base& equation(int) const override;
  Equation_base& equation(int) override;
  void associer_milieu_base(const Milieu_base&) override;
  bool iterateTimeStep(bool& converged) override;

protected:

  Convection_Diffusion_Magnetic_field eq_magnetic_field;
};

#endif /* PB_MHD_TURBULENT_H_ */
