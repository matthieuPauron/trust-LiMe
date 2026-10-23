/****************************************************************************
 * trust-LiMe — Pb_Induction: the standalone magnetic induction problem.
 *
 * This problem solves exactly one equation, the magnetic induction equation
 * Convection_Diffusion_Magnetic_field, on a fluid medium with a *prescribed*
 * transport velocity (constant, analytic field, or the Laguerre/unsteady
 * keyword set of the equation).  There is no momentum equation: the magnetic
 * field is a passive vector advected by a known flow, so the problem
 * deliberately derives from Probleme_base and not from Pb_Fluide_base.
 *
 * Why not Pb_Fluide_base: several kernel source terms treat
 * sub_type(Pb_Fluide_base, pb) as the contract "equation(0) is a
 * Navier_Stokes_std" and ref_cast it unconditionally (e.g.
 * VDF/Sources/Terme_Source_inc_VDF_Face.cpp,
 * VDF/Sources/Terme_Source_Coriolis_VDF_Face.cpp).  An induction-only problem
 * has no momentum equation and must not advertise itself as a fluid problem.
 *
 *     Pb_Induction pb
 *     {
 *         nb_composantes 2
 *         fluide_incompressible { ... }
 *         Convection_Diffusion_Magnetic_field { ... }
 *     }
 *
 * For a coupled hydro + induction problem use Pb_MHD (laminar momentum
 * equation) or Pb_MHD_Turbulent (Navier_Stokes_Turbulent with a k-eps model).
 *
 *  @sa Probleme_base, Convection_Diffusion_Magnetic_field, Pb_MHD
 ****************************************************************************/

#ifndef PB_INDUCTION_H_
#define PB_INDUCTION_H_

#include <Probleme_base.h>
#include <Convection_Diffusion_Magnetic_field.h>

/*! @brief Induction-only problem: one Convection_Diffusion_Magnetic_field
 *         equation and a prescribed transport velocity.
 *
 *  @sa Probleme_base, Pb_Hydraulique, Pb_MHD, Pb_MHD_Turbulent
 */
class Pb_Induction : public Probleme_base
{
  Declare_instanciable(Pb_Induction);

public:

  int nombre_d_equations() const override;
  const Equation_base& equation(int) const override;
  Equation_base& equation(int) override;

protected:

  Convection_Diffusion_Magnetic_field eq_magnetic_field;
};

#endif /* PB_INDUCTION_H_ */
