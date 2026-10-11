/****************************************************************************
 * trust-LiMe — Pb_MHD: standalone magnetic problem.
 ****************************************************************************/

#include <Pb_MHD.h>
#include <Process.h>

Implemente_instanciable(Pb_MHD, "Pb_MHD", Probleme_base);
// XD Pb_MHD Pb_base Pb_MHD INHERITS_BRACE Standalone magnetic problem (magnetic induction equation only).
// XD attr Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field OPT Magnetic induction equation.

Sortie& Pb_MHD::printOn(Sortie& os) const
{
  return Probleme_base::printOn(os);
}

Entree& Pb_MHD::readOn(Entree& is)
{
  return Probleme_base::readOn(is);
}

int Pb_MHD::nombre_d_equations() const
{
  return 1;
}

const Equation_base& Pb_MHD::equation(int i) const
{
  if (i != 0)
    {
      Cerr << "Pb_MHD::equation() : wrong equation index " << i << finl;
      Process::exit();
    }
  return eq_magnetic_field;
}

Equation_base& Pb_MHD::equation(int i)
{
  if (i != 0)
    {
      Cerr << "Pb_MHD::equation() : wrong equation index " << i << finl;
      Process::exit();
    }
  return eq_magnetic_field;
}
