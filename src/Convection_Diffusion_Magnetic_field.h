/****************************************************************************
 * trust-LiMe — magnetic (induction) equation, standalone.
 *
 * Convection-diffusion equation for the magnetic field b:
 *
 *     d b / d t  +  (u . grad) b  -  div( eta grad b )  =  0
 *
 * with eta the magnetic diffusivity (eta = 1 / (mu0 sigma)) and u a given
 * transporting velocity.
 *
 * This class adds as little code as possible: it derives from the kernel
 * class Convection_Diffusion_Temperature_base, which already provides the
 * diffusion/convection operators, the mass solver, the time integration,
 * the boundary-condition and source machinery and the post-processing
 * plumbing.  Only the four magnetic-specific hooks are overridden:
 *
 *   - the unknown field (a cell-centred multi-component "magnetic_field"),
 *   - the diffusivity used by the diffusion operator (eta),
 *   - the transporting velocity,
 *   - the data-file keywords.
 ****************************************************************************/

#ifndef Convection_Diffusion_Magnetic_field_included
#define Convection_Diffusion_Magnetic_field_included

#include <Convection_Diffusion_Temperature_base.h>

/*! @brief Magnetic induction equation (standalone).
 *
 *  @sa Equation_base, Convection_Diffusion_Temperature_base
 */
class Convection_Diffusion_Magnetic_field : public Convection_Diffusion_Temperature_base
{
  Declare_instanciable(Convection_Diffusion_Magnetic_field);

public:

  // --- Data-file handling -------------------------------------------------
  void set_param(Param& param) const override;

  // --- Discretisation and field setup -------------------------------------
  void discretiser() override;

  // --- Unknown ------------------------------------------------------------
  inline const Champ_Inc_base& inconnue() const override { return the_magnetic_field_; }
  inline Champ_Inc_base& inconnue() override { return the_magnetic_field_; }

  // Number of components of the magnetic field (1 <= nb <= space dimension).
  inline int nb_composantes() const { return nb_composantes_; }

  // --- Physical closures used by the inherited operators ------------------
  // Magnetic diffusivity eta: used both as the diffusion coefficient
  // (div(eta grad b)) and for the time-step stability estimate.
  const Champ_Don_base& diffusivite_pour_transport() const override;
  const Champ_base& diffusivite_pour_pas_de_temps() const override;

  // Transporting velocity u.  It must be an "inconnue"-like field: this is the
  // contract imposed by Convection_Diffusion_std::associer_vitesse(), which
  // ref_casts the returned field to Champ_Inc_base.  A zero field is created
  // at discretisation time; it is filled from the optional `vitesse_magnetique`
  // data keyword.
  const Champ_base& vitesse_pour_transport() const override;

  // Reuse the kernel scalar (thermal) boundary conditions.  The thermal
  // scalar CLs (paroi_adiabatique, frontiere_ouverte_temperature_imposee, ...)
  // are then accepted by Cond_lim_base::compatible_avec_eqn().  Physics-specific
  // magnetic CLs are deliberately deferred (see PLAN_magnetic_standalone.md).
  const Motcle& domaine_application() const override;

protected:

  // The magnetic field unknown (vector/multi-component, cell centred).
  OWN_PTR(Champ_Inc_base) the_magnetic_field_;

  // Number of independent components transported.  Defaults to the space
  // dimension (a full vector field); may be reduced (e.g. 1) when the physics
  // of the case involves a single magnetic component (planar/axial flow).
  int nb_composantes_ = -1;

  // Magnetic diffusivity eta, read from the `diffusivite_magnetique` keyword.
  OWN_PTR(Champ_Don_base) diffusivite_magnetique_;

  // Transporting velocity, read from the optional `vitesse_magnetique` keyword.
  OWN_PTR(Champ_Don_base) vitesse_magnetique_;

  // The Champ_Inc actually handed to the convection operator (face centred).
  OWN_PTR(Champ_Inc_base) la_vitesse_transportante_;
};

#endif
