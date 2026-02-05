/*
 * Pb_MHD.cpp
 *
 *  Created on: Feb 3, 2026
 *      Author: matthieu
 */

#include <Pb_MHD.cpp>
#include <Fluide_Incompressible.h>
#include <Champ_Uniforme.h>

Implemente_instanciable(Pb_MHD, "Pb_MHD", Pb_Thermohydraulique);

Sortie& Pb_MHD::printOn(Sortie& os) const { return Pb_Thermohydraulique::printOn(os); }
Entree& Pb_MHD::readOn(Entree& is) { return Pb_Thermohydraulique::readOn(is); }


const Equation_base& Pb_MHD::equation(int i) const
{
  if (i == 0) return eq_hydraulique;
  else if (i == 1) return eq_thermique;
  else if (i == 2) return eq_magnetic_field;
  else
    {
      Cerr << "Pb_Thermohydraulique::equation() : Wrong equation number" << i << "!" << finl;
      Process::exit();
    }
  return eq_hydraulique;
}

Equation_base& Pb_MHD::equation(int i) const
{
  if (i == 0) return eq_hydraulique;
  else if (i == 1) return eq_thermique;
  else if (i == 2) return eq_magnetic_field;
  else
    {
      Cerr << "Pb_Thermohydraulique::equation() : Wrong equation number" << i << "!" << finl;
      Process::exit();
    }
  return eq_hydraulique;
}

void Pb_MHD::associer_milieu_base(const Milieu_base& mil)
{
  Pb_Hydraulique::associer_milieu_base(mil);
  eq_magnetic_field.associer_milieu_base(mil);
}

int Pb_MHD::verifier()
{
  return 1;
}




