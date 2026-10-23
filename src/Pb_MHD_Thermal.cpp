/****************************************************************************
 * trust-LiMe — Pb_MHD_Thermal: hydro + thermal + magnetic induction.
 ****************************************************************************/

#include <Pb_MHD_Thermal.h>
#include <MHD_Coupling.h>
#include <Process.h>
#include <Motcle.h>
#include <Entree_complete.h>
#include <string>

Implemente_instanciable(Pb_MHD_Thermal, "Pb_MHD_Thermal", Pb_Thermohydraulique);
// XD Pb_MHD_Thermal Pb_base Pb_MHD_Thermal INHERITS_BRACE Coupled hydro + thermal + magnetic induction problem:
// XD_CONT Navier_Stokes first, then the energy equation, then the magnetic induction equation.
// XD attr nb_composantes entier nb_composantes OPT Number of transported magnetic components (1..dimension, default dimension).
// XD attr fluide_incompressible fluide_incompressible fluide_incompressible REQ The fluid medium.
// XD attr Navier_Stokes_standard Navier_Stokes_standard Navier_Stokes_standard REQ Laminar momentum equation.
// XD attr Convection_Diffusion_Temperature Convection_Diffusion_Temperature Convection_Diffusion_Temperature REQ Energy equation.
// XD attr Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field REQ Magnetic induction equation.

Sortie& Pb_MHD_Thermal::printOn(Sortie& os) const
{
  return Pb_Thermohydraulique::printOn(os);
}

Entree& Pb_MHD_Thermal::readOn(Entree& is)
{
  Motcle motlu;
  is >> motlu;
  if (motlu != "{")
    Process::exit("Pb_MHD_Thermal: expected '{' to start the problem data block.");

  is >> motlu;
  std::string head("{");
  if (motlu == "nb_composantes")
    {
      int nb = -1;
      is >> nb;
      eq_magnetic_field.set_nb_composantes(nb);
    }
  else
    {
      head += " ";
      head += motlu.getChar();
    }

  Entree_complete ec(head.c_str(), is);
  Pb_Thermohydraulique::readOn(ec);
  return is;
}

int Pb_MHD_Thermal::nombre_d_equations() const
{
  return 3 + eq_opt_.size();
}

const Equation_base& Pb_MHD_Thermal::equation(int i) const
{
  if (i == 0)
    return Pb_Thermohydraulique::equation(0);
  if (i == 1)
    return Pb_Thermohydraulique::equation(1);
  if (i == 2)
    return eq_magnetic_field;
  if (i > 2 && i < 3 + eq_opt_.size())
    return eq_opt_[i - 3].valeur();
  Cerr << "Pb_MHD_Thermal::equation() : wrong equation index " << i
       << " (the problem has " << nombre_d_equations() << " equations)" << finl;
  Process::exit();
  return eq_magnetic_field;
}

Equation_base& Pb_MHD_Thermal::equation(int i)
{
  if (i == 0)
    return Pb_Thermohydraulique::equation(0);
  if (i == 1)
    return Pb_Thermohydraulique::equation(1);
  if (i == 2)
    return eq_magnetic_field;
  if (i > 2 && i < 3 + eq_opt_.size())
    return eq_opt_[i - 3].valeur();
  Cerr << "Pb_MHD_Thermal::equation() : wrong equation index " << i
       << " (the problem has " << nombre_d_equations() << " equations)" << finl;
  Process::exit();
  return eq_magnetic_field;
}

void Pb_MHD_Thermal::associer_milieu_base(const Milieu_base& mil)
{
  Pb_Thermohydraulique::associer_milieu_base(mil);
  eq_magnetic_field.associer_milieu_base(mil);
}

bool Pb_MHD_Thermal::iterateTimeStep(bool& converged)
{
  return mhd_iterate_time_step(*this, converged);
}
