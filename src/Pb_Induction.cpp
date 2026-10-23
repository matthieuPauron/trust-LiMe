/****************************************************************************
 * trust-LiMe — Pb_Induction: the standalone magnetic induction problem.
 ****************************************************************************/

#include <Pb_Induction.h>
#include <Process.h>
#include <Motcle.h>
#include <Entree_complete.h>
#include <string>

Implemente_instanciable(Pb_Induction, "Pb_Induction", Probleme_base);
// XD Pb_Induction Pb_base Pb_Induction INHERITS_BRACE Magnetic induction problem: one transport
// XD_CONT equation for the magnetic field, advected by a prescribed velocity. Use Pb_MHD or
// XD_CONT Pb_MHD_Turbulent when the flow itself is solved.
// XD attr nb_composantes entier nb_composantes OPT Number of transported magnetic components (1..dimension, default dimension).
// XD attr fluide_incompressible fluide_incompressible fluide_incompressible REQ The fluid medium associated with the problem.
// XD attr Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field REQ Magnetic induction equation.

/*! @brief Writes the object identity (standard TRUST I/O). */
Sortie& Pb_Induction::printOn(Sortie& os) const
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
Entree& Pb_Induction::readOn(Entree& is)
{
  Motcle motlu;
  is >> motlu;
  if (motlu != "{")
    Process::exit("Pb_Induction: expected '{' to start the problem data block.");

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

/*! @brief Number of equations of the problem.
 *
 *  Exactly one: the magnetic induction equation.  The standalone problem has
 *  no momentum equation by design (see Pb_Induction.h).
 *
 *  @return 1
 */
int Pb_Induction::nombre_d_equations() const
{
  return 1;
}

/*! @brief Read-only access to equation @p i.
 *
 *  @param[in] i equation index; only 0 (the magnetic induction equation) is
 *             valid.
 *  @return the equation.
 */
const Equation_base& Pb_Induction::equation(int i) const
{
  if (i == 0)
    return eq_magnetic_field;
  Cerr << "Pb_Induction::equation() : wrong equation index " << i
       << " (the problem has 1 equation)" << finl;
  Process::exit();
  return eq_magnetic_field;
}

/*! @brief Writable access to equation @p i (see the const overload). */
Equation_base& Pb_Induction::equation(int i)
{
  if (i == 0)
    return eq_magnetic_field;
  Cerr << "Pb_Induction::equation() : wrong equation index " << i
       << " (the problem has 1 equation)" << finl;
  Process::exit();
  return eq_magnetic_field;
}
