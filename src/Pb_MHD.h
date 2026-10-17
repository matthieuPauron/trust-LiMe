/****************************************************************************
 * trust-LiMe — Pb_MHD: magnetic problem, standalone or low-Rm coupled.
 *
 * Equation 0 is always the magnetic induction equation
 * Convection_Diffusion_Magnetic_field.  Any further Equation_base written in
 * the problem block (typically Navier_Stokes_std for the low-Rm coupled mode,
 * where the Lorentz force 'force_lorentz' source drives the momentum) is read
 * by the kernel's optional-equation mechanism (Probleme_base::read_optional_
 * equations) and held in eq_opt_, exactly like Pb_Hydraulique's optional
 * turbulence equations:
 *
 *     Pb_MHD pb
 *     {
 *         nb_composantes 2
 *         fluide_incompressible { ... }
 *         Convection_Diffusion_Magnetic_field { ... }
 *         Navier_Stokes_standard                # optional; presence => coupled
 *         {
 *             ...
 *             sources { force_lorentz { } }
 *         }
 *     }
 *
 * nombre_d_equations() is 1 standalone, 1 + eq_opt_.size() otherwise.
 * The problem shell stays deliberately thin.
 *
 * ARCHITECTURE NOTE (2026-10-17) — Pb_MHD is ill-named and will be renamed.
 * This class is the *magnetic induction* problem; embedding the momentum
 * equation in it (the optional-equation trick above) is a stopgap for the
 * low-Rm coupled mode.  The target architecture follows the kernel's
 * Probleme_Couple pattern (cf. Pb_Couple_rayo_semi_transp, which couples a
 * Pb_base with a radiation problem): a hydraulics problem (Pb_Hydraulique)
 * solving Navier-Stokes on its own, a renamed magnetic problem (e.g.
 * Pb_Magnetique / Pb_Induction) holding only the induction equation, and a
 * dedicated coupled problem (a Probleme_Couple subclass — the future true
 * Pb_MHD) associating the two.  When that lands:
 *   - rename this class (data-file keyword Pb_MHD becomes the magnetic
 *     problem's new name; the coupled problem takes the Pb_MHD name);
 *   - the magnetic equation's NS locator (eq_navier_stokes(), which walks
 *     probleme().equation(i)) becomes a coupled-problem lookup instead — it
 *     is the single seam of the stopgap to revisit;
 *   - the velocity binding machinery itself (the associer_vitesse rebind in
 *     Convection_Diffusion_Magnetic_field::completer() and the
 *     vitesse_transportante() consumers) is architecture-independent and
 *     carries over unchanged.
 ****************************************************************************/

#ifndef PB_MHD_H_
#define PB_MHD_H_

#include <Probleme_base.h>
#include <Convection_Diffusion_Magnetic_field.h>

/*! @brief Magnetic problem: the magnetic induction equation, optionally
 *         followed by extra equations (e.g. Navier-Stokes for the coupled
 *         low-Rm mode with the Lorentz force).
 *
 *  @sa Probleme_base, Convection_Diffusion_Magnetic_field, Terme_Source_Lorentz
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
