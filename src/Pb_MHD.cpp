/****************************************************************************
 * trust-LiMe — Pb_MHD: standalone magnetic problem.
 ****************************************************************************/

#include <Pb_MHD.h>
#include <Process.h>
#include <Motcle.h>
#include <Entree_complete.h>
#include <string>

Implemente_instanciable(Pb_MHD, "Pb_MHD", Probleme_base);
// XD Pb_MHD Pb_base Pb_MHD INHERITS_BRACE Magnetic problem: magnetic induction equation,
// XD_CONT optionally followed by extra equations (e.g. Navier_Stokes_std for the coupled low-Rm
// XD_CONT mode with the force_lorentz source on the momentum).
// XD attr nb_composantes entier nb_composantes OPT Number of transported magnetic components (1..dimension, default dimension).
// XD attr fluide_incompressible fluide_incompressible fluide_incompressible REQ The fluid medium associated with the problem.
// XD attr Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field REQ Magnetic induction equation.
// XD attr Navier_Stokes_standard Navier_Stokes_standard Navier_Stokes_standard OPT Momentum equation (coupled low-Rm mode); pair with
// XD_CONT the force_lorentz source to apply J x B.

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
  return 1 + eq_opt_.size();
}

const Equation_base& Pb_MHD::equation(int i) const
{
  if (i == 0)
    return eq_magnetic_field;
  if (i > 0 && i < 1 + eq_opt_.size())
    return eq_opt_[i - 1].valeur();
  Cerr << "Pb_MHD::equation() : wrong equation index " << i << " (the problem has "
       << nombre_d_equations() << " equations)" << finl;
  Process::exit();
  return eq_magnetic_field;
}

Equation_base& Pb_MHD::equation(int i)
{
  if (i == 0)
    return eq_magnetic_field;
  if (i > 0 && i < 1 + eq_opt_.size())
    return eq_opt_[i - 1].valeur();
  Cerr << "Pb_MHD::equation() : wrong equation index " << i << " (the problem has "
       << nombre_d_equations() << " equations)" << finl;
  Process::exit();
  return eq_magnetic_field;
}
