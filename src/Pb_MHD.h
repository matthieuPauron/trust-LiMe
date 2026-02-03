/*
 * Pb_MHD.h
 *
 *  Created on: Feb 3, 2026
 *      Author: matthieu
 */

#ifndef PB_MHD_H_
#define PB_MHD_H_

#include <Pb_Thermohydraulique.h>
#include <Convection_Diffusion_Magnetic_field.h>

/*! @brief classe Pb_MHD This class adds a convection diffusion for the magnetic field to the existing TRUST pb_thermohydraulique
 *
 */

class Pb_MHD: public Pb_Thermohydraulique
{
  Declare_instanciable(Pb_MHD);

public:
  int nombre_d_equations() const override { return 1; }
  const Equation_base& equation(int) const override;
  Equation_base& equation(int) override;
  void associer_milieu_base(const Milieu_base&) override;
  int verifier() override;

protected:
  Convection_Diffusion_Magnetic_field eq_magnetic_field;

};

#endif /* PB_MHD_H_ */
