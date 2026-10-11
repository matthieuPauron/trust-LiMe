/****************************************************************************
 * trust-LiMe — Pb_MHD: standalone magnetic problem.
 *
 * One single equation: the magnetic induction equation
 * Convection_Diffusion_Magnetic_field.  No hydraulics, no energy equation.
 * The problem shell is deliberately as thin as Pb_Conduction: it only maps
 * equation(0) to the magnetic equation.
 ****************************************************************************/

#ifndef PB_MHD_H_
#define PB_MHD_H_

#include <Probleme_base.h>
#include <Convection_Diffusion_Magnetic_field.h>

/*! @brief Standalone magnetic problem: a single magnetic induction equation.
 *
 *  @sa Probleme_base, Convection_Diffusion_Magnetic_field, Pb_Conduction
 */
class Pb_MHD : public Probleme_base
{
  Declare_instanciable(Pb_MHD);

public:

  int nombre_d_equations() const override;
  const Equation_base& equation(int) const override;
  Equation_base& equation(int) override;

protected:

  Convection_Diffusion_Magnetic_field eq_magnetic_field;
};

#endif /* PB_MHD_H_ */
