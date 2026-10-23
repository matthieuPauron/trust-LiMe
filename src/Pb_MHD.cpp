/****************************************************************************
 * trust-LiMe — Pb_MHD: hydro + magnetic induction, laminar momentum equation.
 ****************************************************************************/

#include <Pb_MHD.h>
#include <MHD_Coupling.h>
#include <Process.h>
#include <Motcle.h>
#include <Entree_complete.h>
#include <string>

Implemente_instanciable(Pb_MHD, "Pb_MHD", Pb_Hydraulique);
// XD Pb_MHD Pb_base Pb_MHD INHERITS_BRACE Coupled hydro + magnetic induction problem with a laminar
// XD_CONT momentum equation: Navier_Stokes_standard first, then the magnetic induction equation.
// XD attr nb_composantes entier nb_composantes OPT Number of transported magnetic components (1..dimension, default dimension).
// XD attr fluide_incompressible fluide_incompressible fluide_incompressible REQ The fluid medium associated with the problem.
// XD attr Navier_Stokes_standard Navier_Stokes_standard Navier_Stokes_standard REQ Momentum equation; pair with the
// XD_CONT force_lorentz source to apply J x B (b -> u).
// XD attr Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field Convection_Diffusion_Magnetic_field REQ Magnetic induction equation;
// XD_CONT pair with vitesse_hydraulique to use the solved velocity (u -> b).

/*! @brief Writes the object identity (standard TRUST I/O). */
Sortie& Pb_MHD::printOn(Sortie& os) const
{
  return Pb_Hydraulique::printOn(os);
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
 *          Navier_Stokes_standard { ... }
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
  Pb_Hydraulique::readOn(ec);
  return is;
}

/*! @brief Number of equations of the problem.
 *
 *  2 (the momentum equation, then the magnetic induction equation) plus one
 *  per optional extra equation read by the kernel.
 *
 *  @return the equation count (>= 2)
 */
int Pb_MHD::nombre_d_equations() const
{
  return 2 + eq_opt_.size();
}

/*! @brief Read-only access to equation @p i.
 *
 *  @param[in] i equation index: 0 is the momentum equation (inherited from
 *             Pb_Hydraulique), 1 the magnetic induction equation, the
 *             following indices address the optional extra equations in
 *             reading order.
 *  @return the equation.
 */
const Equation_base& Pb_MHD::equation(int i) const
{
  if (i == 0)
    return Pb_Hydraulique::equation(0);
  if (i == 1)
    return eq_magnetic_field;
  if (i > 1 && i < 2 + eq_opt_.size())
    return eq_opt_[i - 2].valeur();
  Cerr << "Pb_MHD::equation() : wrong equation index " << i << " (the problem has "
       << nombre_d_equations() << " equations)" << finl;
  Process::exit();
  return eq_magnetic_field;
}

/*! @brief Writable access to equation @p i (see the const overload). */
Equation_base& Pb_MHD::equation(int i)
{
  if (i == 0)
    return Pb_Hydraulique::equation(0);
  if (i == 1)
    return eq_magnetic_field;
  if (i > 1 && i < 2 + eq_opt_.size())
    return eq_opt_[i - 2].valeur();
  Cerr << "Pb_MHD::equation() : wrong equation index " << i << " (the problem has "
       << nombre_d_equations() << " equations)" << finl;
  Process::exit();
  return eq_magnetic_field;
}

/*! @brief Associates the fluid medium to both solved equations.
 *
 *  Pb_Hydraulique::associer_milieu_base() validates the medium (incompressible
 *  fluid) and binds it to the momentum equation; the magnetic equation then
 *  receives the same medium for its own diffusivity.
 */
void Pb_MHD::associer_milieu_base(const Milieu_base& mil)
{
  Pb_Hydraulique::associer_milieu_base(mil);
  eq_magnetic_field.associer_milieu_base(mil);
}

/*! @brief Time-step iteration, with optional Picard strong coupling.
 *
 *  See mhd_iterate_time_step(): a single fully-lagged sweep by default, a
 *  repeated magnetic <-> momentum sweep when `couplage_fort` is on.
 */
bool Pb_MHD::iterateTimeStep(bool& converged)
{
  return mhd_iterate_time_step(*this, converged);
}
