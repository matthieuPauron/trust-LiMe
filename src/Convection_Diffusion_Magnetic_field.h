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

class Navier_Stokes_std;

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
  /*! @brief The magnetic field unknown (cell-centred, multi-component). */
  inline const Champ_Inc_base& inconnue() const override { return the_magnetic_field_; }
  inline Champ_Inc_base& inconnue() override { return the_magnetic_field_; }

  /*! @brief Number of components of the magnetic field (1 <= nb <= space dimension). */
  inline int nb_composantes() const { return nb_composantes_; }

  /*! @brief Set the component count.  Must be called before discretiser();
   *         the problem classes (Pb_Induction, Pb_MHD, Pb_MHD_Turbulent) read
   *         the problem-level `nb_composantes` keyword and forward it here.
   */
  inline void set_nb_composantes(int nb) { nb_composantes_ = nb; }

  // --- Two-way coupling (coupled low-Rm mode) -------------------------------
  /*! @brief True when the transporting velocity is the velocity unknown of
   *         the Navier-Stokes equation of the same problem (keyword
   *         `vitesse_hydraulique`).  The binding itself is deferred to
   *         completer(): the optional Navier-Stokes equation is created only
   *         after this equation's block has been read
   *         (Probleme_base::read_optional_equations).
   */
  inline bool vitesse_hydraulique() const { return vitesse_hydraulique_; }
  // Strong (Picard) coupling in the two-way mode: iterate the magnetic <->
  // momentum sweeps within a time step until convergence.  Off by default
  // (the historical fully-lagged staggered scheme is used).
  inline bool couplage_fort() const { return couplage_fort_; }
  inline int nb_iterations_couplage() const { return nb_iterations_couplage_ > 0 ? nb_iterations_couplage_ : 8; }
  inline double tolerance_couplage() const { return tolerance_couplage_ > 0. ? tolerance_couplage_ : 1.e-2; }

  /*! @brief The Navier-Stokes equation of the same coupled problem, if any
   *         (nullptr in standalone problems).  Found by walking the problem's
   *         equations, so it works both for Pb_MHD (Navier_Stokes_std) and
   *         Pb_MHD_Turbulent (Navier_Stokes_Turbulent).  The located equation
   *         is cached; the walk is only meaningful once every equation of the
   *         problem has been read (call from completer() or later).
   */
  const Navier_Stokes_std* eq_navier_stokes() const;

  // --- Framework hook -------------------------------------------------------
  /*! @brief Completes the equation (base class), then, in the two-way mode
   *         (`vitesse_hydraulique`), rebinds the transporting velocity to
   *         the Navier-Stokes velocity unknown — see the .cpp for the
   *         read-ordering rationale.
   */
  void completer() override;

  // --- Physical closures used by the inherited operators ------------------
  // Magnetic diffusivity eta: used both as the diffusion coefficient
  // (div(eta grad b)) and for the time-step stability estimate.
  const Champ_Don_base& diffusivite_pour_transport() const override;
  const Champ_base& diffusivite_pour_pas_de_temps() const override;

  // Transporting velocity u.  It must be an "inconnue"-like field: this is the
  // contract imposed by Convection_Diffusion_std::associer_vitesse(), which
  // ref_casts the returned field to Champ_Inc_base.  A zero field is created
  // at discretisation time; it is filled from the optional `vitesse_magnetique`
  // data keyword.  In the two-way mode (`vitesse_hydraulique`) the zero field
  // only serves this read-time association: completer() rebinds the equation
  // and the convection operator to the Navier-Stokes velocity unknown.
  const Champ_base& vitesse_pour_transport() const override;

  // Magnetic (and reused thermal) boundary conditions.  The domain reports
  // Thermique so the kernel scalar (thermal) classes remain accepted
  // (Cond_lim_base::compatible_avec_eqn()), and the dedicated
  //  (homogeneous Neumann) is now declared here
  // (Phase B.1, subdir-free in src/, no new runtime dependency).  Remaining
  // vocabulary: magnetic_field_impose (Dirichlet), paroi_conductrice_magnetique
  // (perfect conductor: b x n = 0; deferred until the per-component CL API is
  // validated -- see Phase B.2 in the roadmap).
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
  /*! @brief Magnetic permeability mu0 (keyword `mu0`, default 1). */
  inline double mu0() const { return mu0_; }
  /*! @brief Charge-to-mass ratio q/m (keyword `rapport_charge_masse`,
   *         default e/m_e).
   */
  inline double rapport_charge_masse() const { return rapport_charge_masse_; }

  // Velocity interpolated at the elements.  The field is face-centred; its
  // element values are auxiliary variables, requested on first need
  // (see KERNEL_REUSE_NOTES.md "Getting a velocity gradient for free").
  void vitesse_aux_elements(DoubleTab& u_elem) const;

  // Element block of the kernel `gradient_vitesse` field, re-extracted in the
  // natural layout: g(e, i*dim + j) = du_i/dx_j.
  void gradient_vitesse_elements(DoubleTab& g) const;

  // Per-element gradient of the magnetic unknown, reconstructed by
  // Green-Gauss: gb(e, i*dim + j) = d b_i / d x_j.  (Used by
  // tension_magnetique and the Lorentz source.)  The table covers ALL
  // elements — local and ghost — so parallel consumers may index it up to
  // nb_elem_tot().  Interior face values are centred averages of the
  // two adjacent elements (exact for linear fields on any mesh); boundary
  // faces take the element's own value (zero normal gradient), which is
  // consistent with the module's only supported wall BCs (Neumann-like).
  void gradient_b_elements(DoubleTab& gb) const;

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
  // Assumed molecular (no turbulent EDM ηₜ): the validation suite uses laminar or
  // prescribed‑mean‑flow velocity fields.  A turbulent contribution
  // (ηₜ from k‑ε → ∇×(u'×b'̄) ≈ −ηₜ∇×B̄) is deferred to END_GOAL.md Phase 3
  // (dynamo threshold); see turbulence section of END_GOAL.md §2.2 and TRUST §3.1.
  OWN_PTR(Champ_Don_base) diffusivite_magnetique_;
  OWN_PTR(Champ_Don_base) diffusivite_turbulente_magnetique_;
  OWN_PTR(Champ_Don_base) diffusivite_effective_;
  // Phase 2b: operands for the turbulent magnetic diffusivity eta_t = C_mu*k^2/eps/Pm_t.
  OWN_PTR(Champ_Don_base) k_magnetique_;
  OWN_PTR(Champ_Don_base) epsilon_magnetique_;
  int k_magnetique_comp_ = 0;
  int epsilon_magnetique_comp_ = 0;
  double C_mu_magnetique_ = 0.09;
  double prandtl_turbulent_magnetique_ = 1.0;
  bool realizable_C_mu_magnetique_ = false;

  // Transporting velocity, read from the optional `vitesse_magnetique` keyword.
  OWN_PTR(Champ_Don_base) vitesse_magnetique_;
  bool ohmic_heating_;
  bool turbulence_mhd_;

  // The Champ_Inc actually handed to the convection operator (face centred).
  // In the two-way mode this placeholder stays zero: completer() rebinds the
  // base vitesse_transportante() (and the convection operator) to the
  // Navier-Stokes velocity unknown, so every velocity consumer must go
  // through vitesse_transportante(), never through this member.
  OWN_PTR(Champ_Inc_base) la_vitesse_transportante_;

  // Two-way coupled mode (keyword `vitesse_hydraulique`): the transporting
  // velocity is the velocity unknown of the Navier-Stokes equation of the
  // same problem, bound in completer().  eq_ns_ caches the located equation
  // (mutable: filled from the const serving path, like
  // vitesse_aux_preparees_ below).
  bool vitesse_hydraulique_ = false;
  bool couplage_fort_ = false;
  int nb_iterations_couplage_ = 0;
  double tolerance_couplage_ = 0.;
  mutable OBS_PTR(Navier_Stokes_std) eq_ns_;

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
  OWN_PTR(Champ_Fonc_base) ch_tension_magnetique_;
  // Differential (grad-b based) fields: curl, divergence, current, Lorentz.
  OWN_PTR(Champ_Fonc_base) ch_rotationnel_magnetique_;
  OWN_PTR(Champ_Fonc_base) ch_divergence_magnetique_;
  OWN_PTR(Champ_Fonc_base) ch_courant_magnetique_;
  OWN_PTR(Champ_Fonc_base) ch_force_lorentz_;

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
