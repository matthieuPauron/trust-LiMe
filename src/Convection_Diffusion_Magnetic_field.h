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
 * plumbing.  Only a few magnetic-specific hooks are overridden:
 *
 *   - the unknown field (a cell-centred multi-component "magnetic_field"),
 *   - the diffusivity used by the diffusion operator (eta),
 *   - the transporting velocity,
 *   - the data-file keywords,
 *   - the post-treatment fields (Champ_Fonc_MHD), see the "Post-treatment
 *     fields" section in docs/content/model.md.
 ****************************************************************************/

#ifndef Convection_Diffusion_Magnetic_field_included
#define Convection_Diffusion_Magnetic_field_included

#include <Convection_Diffusion_Temperature_base.h>

/*! @brief Magnetic induction equation (standalone).
 *
 *  @sa Equation_base, Convection_Diffusion_Temperature_base, Champ_Fonc_MHD
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

  // Set the component count.  Must be called before discretiser(); Pb_MHD
  // reads the problem-level `nb_composantes` keyword and forwards it here.
  inline void set_nb_composantes(int nb) { nb_composantes_ = nb; }

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
  // magnetic CLs are deliberately deferred.  Vocabulary: magnetic_field_impose
  // (Dirichlet), paroi_isolante_magnetique (homogeneous Neumann),
  // paroi_conductrice_magnetique (perfect conductor, b x n = 0).
  const Motcle& domaine_application() const override;

  // --- Post-treatment fields ----------------------------------------------
  // Derived MHD fields, computed on demand by Champ_Fonc_MHD from the current
  // b and from the frozen velocity u (see docs/content/model.md).  Usable by
  // name in `Postraitement { champs { ... } }` and in `Sondes`.
  void creer_champ(const Motcle& motlu) override;
  bool has_champ(const Motcle& nom, OBS_PTR(Champ_base)& ref_champ) const override;
  bool has_champ(const Motcle& nom) const override;
  const Champ_base& get_champ(const Motcle& nom) const override;
  void get_noms_champs_postraitables(Noms& nom, Option opt = NONE) const override;

  // Constants used by the post-treatment fields.
  inline double mu0() const { return mu0_; }
  inline double rapport_charge_masse() const { return rapport_charge_masse_; }

  // Velocity interpolated at the elements.  The field is face-centred; its
  // element values are auxiliary variables, requested on first need
  // (see KERNEL_REUSE_NOTES.md "Getting a velocity gradient for free").
  void vitesse_aux_elements(DoubleTab& u_elem) const;

  // Element block of the kernel `gradient_vitesse` field, re-extracted in the
  // natural layout: g(e, i*dim + j) = du_i/dx_j.
  void gradient_vitesse_elements(DoubleTab& g) const;

  // True if `nom` names one of our derived fields (base or component name).
  bool est_champ_postraitable(const Motcle& nom) const;

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
  bool ohmic_heating_;
  bool turbulence_mhd_;

  // The Champ_Inc actually handed to the convection operator (face centred).
  OWN_PTR(Champ_Inc_base) la_vitesse_transportante_;

  // --- Post-treatment fields (created lazily by creer_champ()) ------------
  double mu0_ = 1.;                          // magnetic permeability mu0 keyword
  double rapport_charge_masse_ = 1.75882001076e11;  // q/m keyword (default e/m_e, C/kg)
  OWN_PTR(Champ_Fonc_base) ch_pulsation_cyclotronique_;
  OWN_PTR(Champ_Fonc_base) ch_energie_magnetique_;
  OWN_PTR(Champ_Fonc_base) ch_vecteur_poynting_;
  OWN_PTR(Champ_Fonc_base) ch_helicite_croisee_;
  OWN_PTR(Champ_Fonc_base) ch_helicite_cinetique_;
  OWN_PTR(Champ_Fonc_base) ch_reynolds_magnetique_;
  OWN_PTR(Champ_Fonc_base) ch_prandtl_magnetique_;

private:

  // One-time preparation of the velocity auxiliary element values and of the
  // kernel `gradient_vitesse` field (used by the u-dependent post-treatment
  // fields only).  Const: also reachable from the const serving path.
  void preparer_vitesse_aux_elems() const;
  void construire_gradient_vitesse() const;
  mutable bool vitesse_aux_preparees_ = false;
  mutable OWN_PTR(Champ_Fonc_base) gradient_vitesse_;  // "gradient_vitesse"

  // Look up our field by (base or component) name.  Returns nullptr when nom
  // is not an MHD post-treatment field.
  const OWN_PTR(Champ_Fonc_base)* champ_postraitable(const Motcle& nom) const;
  OWN_PTR(Champ_Fonc_base)* champ_postraitable(const Motcle& nom);
};

#endif
