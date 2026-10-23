/****************************************************************************
 * trust-LiMe — Pb_MHD: hydro + magnetic induction, laminar momentum equation.
 *
 * Equation 0 is the momentum equation (Navier_Stokes_std, inherited from
 * Pb_Hydraulique), equation 1 is the magnetic induction equation
 * Convection_Diffusion_Magnetic_field, and any further Equation_base written
 * in the problem block is read by the kernel's optional-equation mechanism
 * (Probleme_base::read_optional_equations) and held in eq_opt_.
 *
 *     Pb_MHD pb
 *     {
 *         nb_composantes 2
 *         fluide_incompressible { ... }
 *         Navier_Stokes_standard
 *         {
 *             ...
 *             sources { force_lorentz { } }    # b -> u
 *         }
 *         Convection_Diffusion_Magnetic_field
 *         {
 *             vitesse_hydraulique              # u -> b (solved velocity)
 *             ...
 *         }
 *     }
 *
 * The equation index is part of the contract: equation(0) is always the
 * momentum equation (the medium is discretised through it,
 * typer_lire_milieu), equation(1) is always the magnetic one.  The kernel
 * matches each data-file block by name (Probleme_base::getset_equation_by_name),
 * so the two blocks may be written in either order, but the momentum block is
 * written first by convention, as in Pb_Thermohydraulique /
 * Pb_Thermohydraulique_Turbulent.
 *
 * When a temperature equation is added later (thermal MHD), it must go at
 * index 1 and the magnetic equation last: Convection_Diffusion_Magnetic_field
 * derives from Convection_Diffusion_Temperature, so it would satisfy the
 * sub_type() test that kernel thermal sources such as Terme_Source_inc_th
 * apply to equation(1).
 *
 *  @sa Pb_Hydraulique, Pb_MHD_Turbulent, Pb_Induction,
 *      Convection_Diffusion_Magnetic_field, Terme_Source_Lorentz
 ****************************************************************************/

#ifndef PB_MHD_H_
#define PB_MHD_H_

#include <Pb_Hydraulique.h>
#include <Convection_Diffusion_Magnetic_field.h>

/*! @brief Coupled hydro + magnetic induction problem, laminar momentum
 *         equation.
 *
 *  @sa Pb_Hydraulique, Pb_MHD_Turbulent, Pb_Induction
 */
class Pb_MHD : public Pb_Hydraulique
{
  Declare_instanciable(Pb_MHD);

public:

  int nombre_d_equations() const override;
  const Equation_base& equation(int) const override;
  Equation_base& equation(int) override;
  void associer_milieu_base(const Milieu_base&) override;
  bool iterateTimeStep(bool& converged) override;

protected:

  Convection_Diffusion_Magnetic_field eq_magnetic_field;
};

#endif /* PB_MHD_H_ */
