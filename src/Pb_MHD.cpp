/****************************************************************************
 * trust-LiMe — Pb_MHD: standalone magnetic problem.
 ****************************************************************************/

#include <Pb_MHD.h>
#include <Process.h>
#include <Motcle.h>
#include <Entree_complete.h>
#include <string>

Implemente_instanciable(Pb_MHD, "Pb_MHD", Probleme_base);
// XD Pb_MHD Pb_base Pb_MHD INHERITS_BRACE Standalone magnetic problem (magnetic induction equation only).
// XD attr Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field OPT Magnetic induction equation.

Sortie& Pb_MHD::printOn(Sortie& os) const
{
  return Probleme_base::printOn(os);
}

/*! @brief Reads the problem data block.
 *
 *  The magnetic field component count must be known *before* the inherited
 *  Probleme_base::readOn() discretises the equations (the count is consumed by
 *  Convection_Diffusion_Magnetic_field::discretiser()).  It is therefore
 *  accepted as an optional first keyword of the problem block, ahead of the
 *  medium:
 *
 *      Read pb
 *      {
 *          nb_composantes 1
 *          fluide_incompressible { ... }
 *          Convection_Diffusion_Magnetic_field { ... }
 *      }
 *
 *  The already-read opening brace (and the first token, when it is not
 *  `nb_composantes`) is fed back to the base class through an Entree_complete.
 */
Entree& Pb_MHD::readOn(Entree& is)
{
  Motcle motlu;
  is >> motlu;
  if (motlu != "{")
    Process::exit("Pb_MHD: expected '{' to start the problem data block.");

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
      // Not a component count: hand the first token back to the base class.
      head += " ";
      head += motlu.getChar();
    }

  Entree_complete ec(head.c_str(), is);
  Probleme_base::readOn(ec);
  return is;
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
