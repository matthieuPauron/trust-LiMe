/****************************************************************************
 * trust-LiMe — M1 induction source term: S = (b . grad) u
 *
 * Adds to the right-hand side of the magnetic induction equation the
 * "stretching" term that appears when the induction equation is written in
 * convection-diffusion form for an incompressible flow (the Gauss law
 * transforms the Lorentz form into (u.grad)b on the LHS and (b.grad)u on the
 * RHS):
 *
 *     d b / dt + (u.grad) b - div(eta grad b) = S ,   S = (b.grad) u
 *
 * The velocity u is a *given, frozen* field in the standalone mode (no
 * momentum equation is solved), so its gradient grad u is constant in time
 * and is computed once from the kernel field `gradient_vitesse` (built by
 * PolyMAC_CDO_discretisation::grad_u). In the two-way coupled mode
 * (magnetic-equation keyword `vitesse_hydraulique`) the velocity is the
 * Navier-Stokes unknown and grad u is rebuilt lazily at assembly time by
 * maj_grad_u(), keyed on the velocity's own time (one rebuild per step).
 * In both modes the term is EXPLICIT (lagged) in b, which is the standard
 * treatment for this model.
 *
 * The source lives on the cells (element localisation), matching the cell
 * centred magnetic unknown of Polymac_p0.
 ****************************************************************************/

#ifndef Terme_Source_Induction_stretching_included
#define Terme_Source_Induction_stretching_included

#include <Source_base.h>
#include <TRUST_Ref.h>

class Probleme_base;
class Convection_Diffusion_Magnetic_field;
class Champ_Fonc_base;

/*! @brief Inductive stretching source term S = (b.grad) u for the standalone
 *         magnetic induction equation.
 *
 *  @sa Source_base, Convection_Diffusion_Magnetic_field
 */
class Terme_Source_Induction_stretching : public Source_base
{
  Declare_instanciable(Terme_Source_Induction_stretching);

public:

  // --- I/O (printOn / readOn are declared by Declare_instanciable) ---------

  // --- Framework hooks ----------------------------------------------------
  void associer_pb(const Probleme_base&) override;
  int initialiser(double temps) override;
  void mettre_a_jour(double temps) override;
  void resetTime(double temps) override;

  // --- Source evaluation --------------------------------------------------
  DoubleTab& calculer(DoubleTab& secmem) const override;
  DoubleTab& ajouter(DoubleTab& secmem) const override;

  // --- Bloc interface (Polymac uses the bloc matrix path) -----------------
  void dimensionner_blocs(matrices_t matrices, const tabs_t& semi_impl = {}) const override;
  void ajouter_blocs(matrices_t matrices, DoubleTab& secmem, const tabs_t& semi_impl = {}) const override;

  /*! @brief Declares the bloc-matrix interface (Polymac uses the bloc
   *         matrix path), even though the source is explicit.
   */
  int has_interface_blocs() const override { return 1; }

protected:

  void set_param(Param& param) const override;
  void associer_domaines(const Domaine_dis_base&, const Domaine_Cl_dis_base&) override;

  /*! @brief Adds S = (b.grad) u into `secmem` (already sized like the unknown).
   */
  void ajouter_source(DoubleTab& secmem) const;

  /*! @brief Rebuilds grad(u) when the transport velocity has changed
   *         (two-way mode).  No-op for a frozen data velocity.
   */
  void maj_grad_u() const;

  // The magnetic equation that owns this source.
  OBS_PTR(Convection_Diffusion_Magnetic_field) equation_mag_;

  // grad(u), with dimension*dimension components named dUdX,dUdY[,dVdX,dVdY[,dWdZ]].
  // Mutable: in the two-way mode it is refreshed lazily from the const
  // evaluation path (maj_grad_u), exactly like the velocity caches of the
  // magnetic equation.
  mutable OWN_PTR(Champ_Fonc_base) gradient_vitesse_;

  // Two-way coupling support: when the transporting velocity is the
  // Navier-Stokes unknown (equation keyword vitesse_hydraulique), grad(u)
  // must follow u at every time step instead of being computed once at
  // initialisation.
  bool vitesse_depend_du_temps_ = false;

  // Time of the velocity last used to build gradient_vitesse_ (-1: none).
  mutable double temps_grad_u_ = -1.;

  // Cell centres of the (cell centred) unknown, to guard dimensions.
  int nb_compos_b_ = -1;   // number of components of b actually transported
  int dim_ = -1;           // space dimension
};

#endif