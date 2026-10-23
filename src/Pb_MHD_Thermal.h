/****************************************************************************
 * trust-LiMe — Pb_MHD_Thermal: hydro + thermal + magnetic induction.
 *
 * Equation 0 is the momentum equation (Navier_Stokes_std, inherited from
 * Pb_Hydraulique / Pb_Thermohydraulique), equation 1 is the energy equation
 * (Convection_Diffusion_Temperature, inherited), equation 2 is the magnetic
 * induction equation (Convection_Diffusion_Magnetic_field), and any further
 * Equation_base written in the problem block is read by the kernel's
 * optional-equation mechanism (Probleme_base::read_optional_equations) and
 * held in eq_opt_.
 *
 * The magnetic equation is deliberately kept LAST: it derives from
 * Convection_Diffusion_Temperature, so kernel thermal source terms such as
 * Terme_Source_inc_th, which test sub_type(Convection_Diffusion_Temperature,
 * equation(1)), must see the real temperature equation at index 1.
 *
 *     Pb_MHD_Thermal pb
 *     {
 *         nb_composantes 2
 *         fluide_incompressible { ... }
 *         Navier_Stokes_standard { ... sources { force_lorentz { } } }
 *         Convection_Diffusion_Temperature { ... }
 *         Convection_Diffusion_Magnetic_field { vitesse_hydraulique ... }
 *     }
 *
 *  @sa Pb_Thermohydraulique, Pb_MHD, Pb_MHD_Turbulent, Pb_Induction,
 *      Convection_Diffusion_Magnetic_field, Terme_Source_Lorentz
 ****************************************************************************/

#ifndef PB_MHD_THERMAL_H_
#define PB_MHD_THERMAL_H_

#include <Pb_Thermohydraulique.h>
#include <Convection_Diffusion_Magnetic_field.h>

/*! @brief Coupled hydro + thermal + magnetic induction problem.
 *
 *  @sa Pb_Thermohydraulique, Pb_MHD, Convection_Diffusion_Magnetic_field
 */
class Pb_MHD_Thermal : public Pb_Thermohydraulique
{
  Declare_instanciable(Pb_MHD_Thermal);

public:

  int nombre_d_equations() const override;
  const Equation_base& equation(int) const override;
  Equation_base& equation(int) override;
  void associer_milieu_base(const Milieu_base&) override;
  bool iterateTimeStep(bool& converged) override;

protected:

  Convection_Diffusion_Magnetic_field eq_magnetic_field;
};

#endif /* PB_MHD_THERMAL_H_ */
