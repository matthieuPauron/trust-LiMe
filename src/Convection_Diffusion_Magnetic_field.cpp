/****************************************************************************
 * trust-LiMe — magnetic (induction) equation, standalone.
 * See Convection_Diffusion_Magnetic_field.h for the design notes.
 ****************************************************************************/

#include <Convection_Diffusion_Magnetic_field.h>
#include <Champ_Fonc_MHD.h>
#include <Champ_Fonc_Somme.h>
#include <Champ_Fonc_Eta_Turbulent.h>
#include <Probleme_base.h>
#include <Discret_Thyd.h>
#include <Param.h>
#include <Process.h>
#include <Champ_Inc_base.h>
#include <Champ_Don_base.h>
#include <Champ_Uniforme.h>
#include <Champ_Face_PolyMAC_MPFA.h>
#include <Fluide_base.h>
#include <Navier_Stokes_std.h>
#include <Navier_Stokes_Turbulent.h>
#include <Modele_turbulence_hyd_base.h>
#include <Modele_turbulence_hyd_K_Eps.h>
#include <Domaine_VF.h>
#include <Domaine_Cl_dis_base.h>

Implemente_instanciable(Convection_Diffusion_Magnetic_field,
                        "Convection_Diffusion_Magnetic_field",
                        Convection_Diffusion_Temperature_base);
// XD Convection_Diffusion_Magnetic_field eqn_base Convection_Diffusion_Magnetic_field INHERITS_BRACE
// XD_CONT Standalone convection-diffusion (induction) equation for the magnetic field b:
// XD_CONT d b / d t + (u . grad) b - div(eta grad b) = 0.
// XD attr diffusivite_magnetique field_base diffusivite_magnetique REQ Magnetic diffusivity eta = 1/(mu0 sigma).
// XD attr diffusivite_turbulente_magnetique field_base diffusivite_turbulente_magnetique OPT Turbulent magnetic diffusivity eta_t. Folded into eta: uniform fields are summed into a uniform eta_eff at read time (Phase 1); non-uniform fields are summed point-wise into an effective field in completer() (Phase 2a).
// XD attr k_magnetique field_base k_magnetique OPT Turbulent kinetic energy k of the magnetic fluctuations (Phase 2b operand). Used with epsilon_magnetique to build eta_t = C_mu*k^2/eps/Pm_t.
// XD attr epsilon_magnetique field_base epsilon_magnetique OPT Dissipation rate epsilon of the magnetic fluctuations (Phase 2b operand).
// XD attr C_mu_magnetique floattant C_mu_magnetique OPT Constant C_mu for the magnetic eddy diffusivity (default 0.09).
// XD attr prandtl_turbulent_magnetique floattant prandtl_turbulent_magnetique OPT Turbulent magnetic Prandtl number Pm_t (default 1.0).
// XD attr vitesse_magnetique field_base vitesse_magnetique OPT Transporting velocity u (default: zero).
// XD attr nb_composantes entier nb_composantes OPT Number of transported components of the magnetic field (default: the
// XD_CONT space dimension). Use 1 for a single magnetic component.
// XD attr mu0 floattant mu0 OPT Magnetic permeability mu_0, used by the post-treatment fields
// XD_CONT (energie_magnetique, vecteur_poynting). Default: 1.
// XD attr rapport_charge_masse floattant rapport_charge_masse OPT Charge-to-mass ratio q/m for the
// XD_CONT pulsation_cyclotronique post-treatment field. Default: e/m_e = 1.75882001076e11 C/kg
// XD_CONT (electron mass, positron charge).
// XD attr vitesse_hydraulique rien vitesse_hydraulique OPT Two-way coupled mode: take the transporting
// XD_CONT velocity u from the Navier_Stokes_standard equation of the same problem. Mutually exclusive
// XD_CONT with vitesse_magnetique; requires Navier_Stokes_standard in the problem block.

/*! @brief Writes the object identity (standard TRUST I/O). */
Sortie& Convection_Diffusion_Magnetic_field::printOn(Sortie& os) const
{
  return Convection_Diffusion_Temperature_base::printOn(os);
}

/*! @brief Reads the magnetic equation data block.
 *
 *  Reuses the kernel reading machinery: after the inherited read (which reads
 *  diffusion/convection, sources, boundary and initial conditions), the
 *  transporting velocity is filled from the optional data field, and the mass
 *  solver coefficient is renamed.
 */
Entree& Convection_Diffusion_Magnetic_field::readOn(Entree& is)
{
  assert(the_magnetic_field_);
  Convection_Diffusion_Temperature_base::readOn(is);

  // The convection operator was associated with la_vitesse_transportante_
  // (a Champ_Inc) while the block was read; fill it now from the data field.
  if (vitesse_magnetique_)
    la_vitesse_transportante_->affecter(vitesse_magnetique_.valeur());

  // Two-way mode: vitesse_hydraulique excludes the data velocity.  (The
  // placeholder field stays zero: completer() rebinds the transport velocity
  // to the Navier-Stokes unknown once every equation is known.)
  if (vitesse_hydraulique_ && vitesse_magnetique_)
    Process::exit("Convection_Diffusion_Magnetic_field: 'vitesse_hydraulique' (velocity "
                  "taken from the Navier-Stokes equation) and 'vitesse_magnetique' "
                  "(data velocity) are mutually exclusive; keep only one of them.");

  // The magnetic equation is a pure evolution equation: d b / d t.  Its time
  // term must therefore use the identity mass matrix, i.e. no coefficient
  // field.  In particular, the previous value "magnetic_field" was the
  // *unknown's own name*, which made Solveur_Masse_base::appliquer() divide the
  // mass term by b: the scheme became nonlinear and the measured diffusion
  // rate diverged from eta (and from the analytic exp(-eta k^2 t)).
  if (solveur_masse)
    solveur_masse->set_name_of_coefficient_temporel("no_coeff");

  // Induction turbulence: fold a turbulent magnetic diffusivity into an
  // effective diffusivity eta_eff = eta + eta_t.
  //   Phase 1: both fields uniform -> summed into a uniform Champ_Uniforme here.
  //   Phase 2a: non-uniform fields -> summed point-wise in completer() into a
  //   Champ_Fonc_Somme (eta_eff(x) = eta(x) + eta_t(x)).
  if (diffusivite_turbulente_magnetique_)
    {
      const Champ_Uniforme* eta_u = dynamic_cast<const Champ_Uniforme*>(&diffusivite_magnetique_.valeur());
      const Champ_Uniforme* etat_u = dynamic_cast<const Champ_Uniforme*>(&diffusivite_turbulente_magnetique_.valeur());
      if (eta_u && etat_u)
        {
          const double eta = eta_u->valeurs()(0, 0);
          const double etat = etat_u->valeurs()(0, 0);
          diffusivite_effective_.typer("Champ_Uniforme");
          Champ_Uniforme& eff = ref_cast(Champ_Uniforme, diffusivite_effective_.valeur());
          eff.valeurs().resize(1, 1);
          eff.valeurs()(0, 0) = eta + etat;
          Cerr << "Convection_Diffusion_Magnetic_field: turbulent magnetic diffusivity folded, "
               << "eta_eff = " << (eta + etat) << " (eta=" << eta << ", eta_t=" << etat << ")" << finl;
        }
      else
        {
          Cerr << "Convection_Diffusion_Magnetic_field: 'diffusivite_turbulente_magnetique' "
               << "non-uniform case deferred to completer() (Phase 2a point-wise fold)." << finl;
        }
    }

  return is;
}

/*! @brief Registers the magnetic keywords on top of the inherited ones. */
void Convection_Diffusion_Magnetic_field::set_param(Param& param) const
{
  Convection_Diffusion_Temperature_base::set_param(param);
  // NOTE: `diffusivite_magnetique` must appear before `diffusion` in the data
  // file (it is queried when the diffusion operator is read).
  param.ajouter("diffusivite_magnetique", &diffusivite_magnetique_);
  param.ajouter("diffusivite_turbulente_magnetique", &diffusivite_turbulente_magnetique_);
  // Phase 2b: build eta_t = C_mu*k^2/eps/Pm_t from k, eps fields.
  param.ajouter("k_magnetique", &k_magnetique_);
  param.ajouter("epsilon_magnetique", &epsilon_magnetique_);
  param.ajouter("C_mu_magnetique", &C_mu_magnetique_);
  param.ajouter("prandtl_turbulent_magnetique", &prandtl_turbulent_magnetique_);
  param.ajouter_flag("realizable_C_mu_magnetique", &realizable_C_mu_magnetique_);
  param.ajouter("vitesse_magnetique", &vitesse_magnetique_);
  // Constants consumed by the post-treatment fields (Champ_Fonc_MHD) only;
  // they have no influence on the transport itself.  See
  // docs/content/model.md ("Post-treatment fields") for the conventions.
  param.ajouter("mu0", &mu0_);
  param.ajouter("rapport_charge_masse", &rapport_charge_masse_);
  // Two-way coupled low-Rm mode: the transporting velocity is the velocity
  // unknown of the Navier_Stokes_standard equation of the same problem.
  // Bare flag keyword (Param::ajouter_flag, like the kernel's
  // postraiter_gradient_pression_sans_masse).  The actual binding is deferred
  // to completer(): the optional Navier-Stokes equation does not exist yet
  // while this block is read (Probleme_base::read_optional_equations).
  param.ajouter_flag("vitesse_hydraulique", &vitesse_hydraulique_);
  // Strong (Picard) coupling within the two-way mode (opt-in).
  param.ajouter_flag("couplage_fort", &couplage_fort_);
  param.ajouter("nb_iterations_couplage", &nb_iterations_couplage_);
  param.ajouter("tolerance_couplage", &tolerance_couplage_);
  // NOTE: `nb_composantes` is NOT an equation keyword: it must be known before
  // the equations are discretised, so it is read at the *problem* level by
  // Pb_MHD::readOn() (ahead of the medium) and pushed here via
  // set_nb_composantes().
}

/*! @brief Builds the unknown and the transporting velocity fields.
 *
 *  Current scope: up to `dimension` transported components
 *  ("magnetic_field"), discretised with the concentration (multi-scalar
 *  concentration) machinery of Discret_Thyd.
 */
void Convection_Diffusion_Magnetic_field::discretiser()
{
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, discretisation());
  Cerr << "Magnetic field equation discretization" << finl;

  // Nb_composantes must be known before discretisation (read at the problem
  // level by Pb_MHD::readOn()); default: one component per space direction.
  if (nb_composantes_ < 0)
    nb_composantes_ = dimension;
  if (nb_composantes_ < 1 or nb_composantes_ > dimension)
    Process::exit("Convection_Diffusion_Magnetic_field: nb_composantes must be "
                  "between 1 and the space dimension.");
  // Magnetic field: one scalar component.
  dis.concentration(schema_temps(), domaine_dis(), the_magnetic_field_,
                    nb_composantes_, "magnetic_field");
  champs_compris_.ajoute_champ(the_magnetic_field_);

  // Transporting velocity: a vector field, zero unless overridden by data.
  dis.vitesse(schema_temps(), domaine_dis(), la_vitesse_transportante_, 1);
  la_vitesse_transportante_->nommer("vitesse_magnetique");
  // The transporting velocity is not the unknown of any equation, but the
  // convection operator queries its equation() (through MorEqn) to reach the
  // boundary-condition descriptors (Champ_Face_base::init_fcl).  Associate it
  // with this equation, exactly as the unknown is associated by the kernel.
  la_vitesse_transportante_->associer_eqn(*this);

  Equation_base::discretiser();

  Cerr << "Convection_Diffusion_Magnetic_field::discretiser() ok" << finl;
}

/*! @brief The magnetic diffusivity eta (required keyword `diffusivite_magnetique`).
 *
 *  @return the diffusivity field; the process exits cleanly when the keyword
 *          was omitted (it cannot be defaulted: it sets the physical decay
 *          rate of the equation).
 */
const Champ_Don_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_transport() const
{
  if (diffusivite_effective_)
    return diffusivite_effective_.valeur();
  if (!diffusivite_magnetique_)
    Process::exit("Convection_Diffusion_Magnetic_field: the keyword 'diffusivite_magnetique' "
                  "(magnetic diffusivity eta) is required and must be given before 'diffusion'.");
  return diffusivite_magnetique_.valeur();
}

/*! @brief Diffusivity used by the time-step stability estimate: eta. */
const Champ_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_pas_de_temps() const
{
  if (diffusivite_effective_)
    return diffusivite_effective_.valeur();
  return diffusivite_pour_transport();
}

/*! @brief The transporting velocity u handed to the convection operator. */
const Champ_base& Convection_Diffusion_Magnetic_field::vitesse_pour_transport() const
{
  if (!la_vitesse_transportante_)
    Process::exit("Convection_Diffusion_Magnetic_field: transporting velocity not discretized.");
  return la_vitesse_transportante_.valeur();
}

/*! @brief Finds the Navier-Stokes equation of the hosting problem, if any.
 *
 *  Same walk as Terme_Source_Lorentz::associer_pb(): scan the problem's
 *  equations (index 0 is this magnetic equation, the optional equations
 *  follow) and keep the first Navier_Stokes_std.  Only meaningful once
 *  every equation has been read; the result is cached in eq_ns_.
 *
 *  @return the Navier-Stokes equation, or nullptr in standalone problems.
 */
const Navier_Stokes_std* Convection_Diffusion_Magnetic_field::eq_navier_stokes() const
{
  if (eq_ns_)
    return &eq_ns_.valeur();
  const int nb_eq = probleme().nombre_d_equations();
  for (int i = 0; i < nb_eq; i++)
    {
      const Equation_base& eqn = probleme().equation(i);
      if (sub_type(Navier_Stokes_std, eqn))
        {
          eq_ns_ = ref_cast(Navier_Stokes_std, eqn);
          return &eq_ns_.valeur();
        }
    }
  return nullptr;
}

/*! @brief Completes the equation, then binds the two-way transport velocity.
 *
 *  In the two-way mode (keyword `vitesse_hydraulique`), the (zero)
 *  placeholder velocity that served the read-time association is replaced by
 *  the velocity unknown of the Navier-Stokes equation of the same problem.
 *
 *  Why here and not at read time: in Pb_MHD / Pb_MHD_Turbulent the momentum
 *  equation is equation 0 and is read *before* the magnetic block, but the
 *  binding is still deferred to completer() so that a standalone Pb_Induction
 *  (no momentum equation at all) is handled by the same code path.  completer()
 *  runs once every equation is known.
 *
 *  Both the equation-level pointer (associer_vitesse, used by the
 *  vitesse_transportante() consumers such as the induction_stretching
 *  source) and the convection operator are re-bound below; the PolyMAC
 *  evaluator re-references the velocity values array on every flux
 *  evaluation (Eval_Conv_PolyMAC_CDO::mettre_a_jour), so the operator
 *  always reads the current NS velocity slot.
 */
void Convection_Diffusion_Magnetic_field::completer()
{
  Convection_Diffusion_Temperature_base::completer();

  // Phase 4: if no explicit k/eps fields but a hydraulic turbulence model is
  // present on the Navier-Stokes equation, grab its k and epsilon fields and
  // build eta_t = C_mu*k^2/eps/Pm_t (TrioCFD k-eps).
  if (vitesse_hydraulique_ && !k_magnetique_ && !epsilon_magnetique_ && !diffusivite_turbulente_magnetique_)
    {
      const Navier_Stokes_std* ns = eq_navier_stokes();
      const Navier_Stokes_Turbulent* nst = dynamic_cast<const Navier_Stokes_Turbulent*>(ns);
      if (nst)
        {
          const Modele_turbulence_hyd_base& mt = nst->modele_turbulence();
          OBS_PTR(Champ_base) k_ref, e_ref;
          bool has_k = mt.has_champ("k", k_ref) || mt.has_champ("K", k_ref);
          bool has_e = mt.has_champ("epsilon", e_ref) || mt.has_champ("eps", e_ref) || mt.has_champ("Epsilon", e_ref);
          int kc = 0, ec = 0;
          if (!has_k || !has_e)
            {
              // Fall back to the 2-component K_Eps transport field.
              OBS_PTR(Champ_base) ke;
              if (mt.has_champ("K_Eps", ke))
                {
                  k_ref = ke;
                  e_ref = ke;
                  has_k = has_e = true;
                  kc = 0;
                  ec = 1;
                }
            }
          if (has_k && has_e)
            {
              // Build the eddy field directly (operands are OBS_PTR -> Champ_base&).
              OWN_PTR(Champ_Fonc_base) eddy;
              eddy.typer("Champ_Fonc_Eta_Turbulent");
              Champ_Fonc_Eta_Turbulent& e = ref_cast(Champ_Fonc_Eta_Turbulent, eddy.valeur());
              e.configurer(*this, k_ref.valeur(), kc, e_ref.valeur(), ec,
                           C_mu_magnetique_, prandtl_turbulent_magnetique_,
                           realizable_C_mu_magnetique_);
              diffusivite_turbulente_magnetique_ = eddy;
              champs_compris_.ajoute_champ(diffusivite_turbulente_magnetique_);
              // Evaluate the field once and report its range: a spatially
              // varying hydraulic k/eps must show up as a non-uniform eta_t.
              diffusivite_turbulente_magnetique_->mettre_a_jour(inconnue().temps());
              const DoubleTab& etav = diffusivite_turbulente_magnetique_->valeurs();
              double eta_min = 0., eta_max = 0.;
              if (etav.size() > 0)
                {
                  eta_min = eta_max = etav(0, 0);
                  for (int i = 0; i < etav.dimension(0); i++)
                    for (int c = 0; c < etav.dimension(1); c++)
                      {
                        if (etav(i, c) < eta_min) eta_min = etav(i, c);
                        if (etav(i, c) > eta_max) eta_max = etav(i, c);
                      }
                }
              Cerr << "Convection_Diffusion_Magnetic_field: hydraulic turbulence model detected, "
                   << "using its k/epsilon for the magnetic eddy diffusivity "
                   << "(eta_t in [" << eta_min << ", " << eta_max << "])." << finl;
            }
        }
    }

  // Phase 2b: build eta_t = C_mu*k^2/eps/Pm_t from k, epsilon fields (if given)
  // and store it as diffusivite_turbulente_magnetique_, so the Phase 2a fold below
  // handles the rest.
  if (k_magnetique_ && epsilon_magnetique_ && !diffusivite_turbulente_magnetique_)
    {
      OWN_PTR(Champ_Fonc_base) eddy;
      eddy.typer("Champ_Fonc_Eta_Turbulent");
      Champ_Fonc_Eta_Turbulent& e = ref_cast(Champ_Fonc_Eta_Turbulent, eddy.valeur());
      e.configurer(*this, k_magnetique_.valeur(), k_magnetique_comp_,
                   epsilon_magnetique_.valeur(), epsilon_magnetique_comp_,
                   C_mu_magnetique_, prandtl_turbulent_magnetique_,
                   realizable_C_mu_magnetique_);
      diffusivite_turbulente_magnetique_ = eddy;
      champs_compris_.ajoute_champ(diffusivite_turbulente_magnetique_);
      Cerr << "Convection_Diffusion_Magnetic_field: built turbulent magnetic diffusivity "
           << "eta_t = C_mu*k^2/eps/Pm_t (C_mu=" << C_mu_magnetique_
           << ", Pm_t=" << prandtl_turbulent_magnetique_ << ")." << finl;
    }

  // Phase 2 induction turbulence: when a turbulent magnetic diffusivity was given
  // but could not be folded at readOn (non-uniform fields), build an effective
  // diffusivity field eta_eff(x) = eta(x) + eta_t(x) as a point-wise sum.
  if (diffusivite_turbulente_magnetique_ && !diffusivite_effective_)
    {
      const Champ_Uniforme* eta_u = dynamic_cast<const Champ_Uniforme*>(&diffusivite_magnetique_.valeur());
      const Champ_Uniforme* etat_u = dynamic_cast<const Champ_Uniforme*>(&diffusivite_turbulente_magnetique_.valeur());
      if (!(eta_u && etat_u))
        {
          OWN_PTR(Champ_Fonc_base) somme;
          somme.typer("Champ_Fonc_Somme");
          Champ_Fonc_Somme& s = ref_cast(Champ_Fonc_Somme, somme.valeur());
          s.configurer(*this, diffusivite_magnetique_.valeur(),
                       diffusivite_turbulente_magnetique_.valeur());
          diffusivite_effective_ = somme;
          // Register so the problem-level update keeps its values fresh.
          champs_compris_.ajoute_champ(diffusivite_effective_);
          Cerr << "Convection_Diffusion_Magnetic_field: turbulent magnetic diffusivity folded "
               << "(non-uniform) into eta_eff = eta + eta_t." << finl;
        }
    }

  if (!vitesse_hydraulique_)
    return;

  const Navier_Stokes_std* ns = eq_navier_stokes();
  if (!ns)
    {
      Cerr << "Convection_Diffusion_Magnetic_field: the keyword 'vitesse_hydraulique'" << finl;
      Cerr << "requires a momentum equation in the same problem: Navier_Stokes_standard" << finl;
      Cerr << "in Pb_MHD, or Navier_Stokes_turbulent in Pb_MHD_Turbulent." << finl;
      Process::exit();
    }

  // Rebind the equation-level pointer and the convection operator to the
  // solved velocity.  TRAP (found by the TwoWayFeedback calibration): the
  // Operateur_Conv *wrapper*'s associer_vitesse() only stores its own
  // la_vitesse observer; the forward to the underlying operator happens
  // once, in Operateur_Conv::typer() (read time, with the placeholder).
  // After typer(), the operator must be re-bound through
  // Operateur_Conv_base::associer_vitesse(), i.e. terme_convectif.valeur(),
  // or it keeps evaluating with the placeholder velocity.  The wrapper call
  // is kept for its record: vitesse() is re-read if the operator is ever
  // re-typed.  With a 'convection negligeable' operator both calls are
  // no-ops, but the equation-level pointer is still rebound, so the sources
  // see the NS velocity.
  associer_vitesse(ns->vitesse());
  terme_convectif.associer_vitesse(ns->vitesse());
  terme_convectif.valeur().associer_vitesse(ns->vitesse());

  // Drop the one-time caches possibly built against the placeholder so that
  // vitesse_aux_elements() / gradient_vitesse_elements() (and the
  // u-dependent post-treatment fields: helicite_cinetique,
  // nombre_de_reynolds_magnetique, ...) rebuild against the NS velocity.
  // Defensive: in practice the first preparation happens after completer()
  // (all consumers are lazy, post-read) — do not remove.
  vitesse_aux_preparees_ = false;
  gradient_vitesse_ = OWN_PTR(Champ_Fonc_base)();

  Cerr << "Convection_Diffusion_Magnetic_field: two-way coupling enabled, transporting "
       << "velocity taken from the " << ns->que_suis_je() << " equation." << finl;
}

/*! @brief Reuses the kernel scalar (thermal) boundary conditions.
 *
 *  Reporting "Thermique" makes the standard scalar CLs compatible
 *  (Cond_lim_base::compatible_avec_eqn).  Dedicated magnetic CLs are deferred.
 */
const Motcle& Convection_Diffusion_Magnetic_field::domaine_application() const
{
  static Motcle domaine = "Thermique";
  return domaine;
}

// ---------------------------------------------------------------------------
// Post-treatment fields
// ---------------------------------------------------------------------------

/*! @brief Maps a post-treatment field name to its storage member.
 *
 *  Returns nullptr when `nom` is not one of the 7 derived MHD fields.  A
 *  component request (e.g. "vecteur_poynting_x") is mapped to the full field.
 */
const OWN_PTR(Champ_Fonc_base)* Convection_Diffusion_Magnetic_field::champ_postraitable(const Motcle& nom) const
{
  if (nom == "pulsation_cyclotronique")
    return &ch_pulsation_cyclotronique_;
  if (nom == "energie_magnetique")
    return &ch_energie_magnetique_;
  if (nom == "helicite_croisee")
    return &ch_helicite_croisee_;
  if (nom == "helicite_cinetique")
    return &ch_helicite_cinetique_;
  if (nom == "nombre_de_reynolds_magnetique")
    return &ch_reynolds_magnetique_;
  if (nom == "nombre_de_prandtl_magnetique")
    return &ch_prandtl_magnetique_;
  if (nom == "tension_magnetique")
    return &ch_tension_magnetique_;
  if (nom == "rotationnel_magnetique")
    return &ch_rotationnel_magnetique_;
  if (nom == "divergence_magnetique")
    return &ch_divergence_magnetique_;
  if (nom == "courant_magnetique")
    return &ch_courant_magnetique_;
  if (nom == "force_lorentz")
    return &ch_force_lorentz_;
  // Vector field and its components ("vecteur_poynting_x", ...).
  if (nom.debute_par("vecteur_poynting"))
    return &ch_vecteur_poynting_;
  return nullptr;
}

/*! @brief Non-const overload (see the const one); used by creer_champ(). */
OWN_PTR(Champ_Fonc_base)* Convection_Diffusion_Magnetic_field::champ_postraitable(const Motcle& nom)
{
  // Owns and mutates the members, so this overload is used by creer_champ().
  const Convection_Diffusion_Magnetic_field& me = *this;
  return const_cast<OWN_PTR(Champ_Fonc_base)*>(me.champ_postraitable(nom));
}

/*! @brief True when `nom` names one of the derived MHD fields.
 *
 *  @param[in] nom field name (base name or component name).
 *  @return true if the name is a post-treatment field of this equation.
 */
bool Convection_Diffusion_Magnetic_field::est_champ_postraitable(const Motcle& nom) const
{
  return champ_postraitable(nom) != nullptr;
}

/*! @brief Reports our derived fields (route through get_champ() so that the
 *         values are recomputed when stale).
 *
 *  The kernel post-processing paths resolve a field via
 *  Probleme_base::get_champ() -> Equation_base::has_champ(nom, ref), whose
 *  default implementation returns the registered field *without* recomputing
 *  it.  Recomputing happens here instead (same pattern as
 *  Navier_Stokes_std::has_champ).
 */
bool Convection_Diffusion_Magnetic_field::has_champ(const Motcle& nom, OBS_PTR(Champ_base)& ref_champ) const
{
  const OWN_PTR(Champ_Fonc_base)* champp = champ_postraitable(nom);
  if (champp and *champp)
    {
      ref_champ = get_champ(nom);
      return true;
    }
  return Convection_Diffusion_Temperature_base::has_champ(nom, ref_champ);
}

/*! @brief Field-existence query for the derived MHD fields. */
bool Convection_Diffusion_Magnetic_field::has_champ(const Motcle& nom) const
{
  if (est_champ_postraitable(nom))
    return true;
  return Convection_Diffusion_Temperature_base::has_champ(nom);
}

/*! @brief Lazily creates the requested derived field and registers it.
 *
 *  Called through Probleme_base::creer_champ() while the Postraitement block
 *  is being read (both the `champs { ... }` and the `Sondes { ... }` paths).
 *  The u-dependent fields also trigger the one-time preparation of the
 *  velocity auxiliary element values (and of the element gradient), exactly
 *  like the induction_stretching source term (see KERNEL_REUSE_NOTES.md).
 */
void Convection_Diffusion_Magnetic_field::creer_champ(const Motcle& motlu)
{
  Convection_Diffusion_Temperature_base::creer_champ(motlu);

  OWN_PTR(Champ_Fonc_base)* champp = champ_postraitable(motlu);
  if (!champp or *champp)
    return;                                        // not ours, or already created

  // A component request ("vecteur_poynting_x") creates the full base field.
  const Motcle kind = motlu.debute_par("vecteur_poynting") ? Motcle("vecteur_poynting") : motlu;

  // The grad-b based post-treatment fields act on the full vector magnetic field.
  // The scalar model (nb_composantes 1) transports a single component and their
  // complete form is not meaningful, so they are rejected rather than
  // silently emitted (mirrors Champ_Fonc_MHD's calculer() guard).
  const bool champ_grad_b = (kind == "tension_magnetique" || kind == "rotationnel_magnetique"
                             || kind == "divergence_magnetique" || kind == "courant_magnetique"
                             || kind == "force_lorentz");
  if (champ_grad_b && nb_composantes() != dimension)
    {
      Cerr << "Convection_Diffusion_Magnetic_field: the post-treatment field '"
           << kind << "' requires a full vector magnetic field "
           << "(nb_composantes = dimension in the problem block), while this "
           << "case transports nb_composantes = " << nb_composantes()
           << ".  Please set nb_composantes accordingly or drop the field "
           << "from the Postraitement block." << finl;
      Process::exit();
    }

  champp->typer("Champ_Fonc_MHD");

  Champ_Fonc_MHD& ch = ref_cast(Champ_Fonc_MHD, champp->valeur());
  ch.configurer(*this, kind);
  champs_compris_.ajoute_champ(*champp);
}

/*! @brief Serves the derived fields, recomputing them if stale.
 *
 *  Every consumer (Postraitement's `champs` / `Sondes` / `Statistiques`
 *  machinery) goes through Probleme_base::get_champ -> equation get_champ, so
 *  recomputing here (when the field's time differs from the unknown's time)
 *  covers every read path.
 */
const Champ_base& Convection_Diffusion_Magnetic_field::get_champ(const Motcle& nom) const
{
  const OWN_PTR(Champ_Fonc_base)* champp = champ_postraitable(nom);
  if (champp and *champp)
    {
      Champ_Fonc_MHD& ch = ref_cast_non_const(Champ_Fonc_MHD, champp->valeur());
      if (ch.temps() != inconnue().temps())
        ch.mettre_a_jour(inconnue().temps());
      // else: values already computed for this time step.
      return Convection_Diffusion_Temperature_base::get_champ(nom);
    }

  return Convection_Diffusion_Temperature_base::get_champ(nom);
}

/*! @brief Advertises the derived fields so that the Postraitement/Sondes
 *         machinery recognises their names (set_noms_champs_postraitables()
 *         runs *before* the fields are created).
 */
void Convection_Diffusion_Magnetic_field::get_noms_champs_postraitables(Noms& nom, Option opt) const
{
  Convection_Diffusion_Temperature_base::get_noms_champs_postraitables(nom, opt);

  Noms noms_compris;
  noms_compris.add("pulsation_cyclotronique");
  noms_compris.add("energie_magnetique");
  noms_compris.add("vecteur_poynting");
  noms_compris.add("helicite_croisee");
  noms_compris.add("helicite_cinetique");
  noms_compris.add("nombre_de_reynolds_magnetique");
  noms_compris.add("nombre_de_prandtl_magnetique");
  noms_compris.add("tension_magnetique");
  noms_compris.add("rotationnel_magnetique");
  noms_compris.add("divergence_magnetique");
  noms_compris.add("courant_magnetique");
  noms_compris.add("force_lorentz");

  if (opt == DESCRIPTION)
    Cerr << que_suis_je() << " : " << noms_compris << finl;
  else
    nom.add(noms_compris);
}

/*! @brief Requests the velocity element values (auxiliary variables) on the
 *         transporting face field; idempotent.
 *
 *  grad_Champ_Face_PolyMAC_MPFA::calc_gradfve() and valeur_aux_elems() read
 *  the velocity's element values from the extended rows [nb_faces_tot + e*dim
 *  + d].  Those rows only exist after init_auxiliary_variables(); with an
 *  Elem unknown nobody requests them, so they must be requested here
 *  explicitly or the reads return out-of-bounds garbage (see
 *  KERNEL_REUSE_NOTES.md "Getting a velocity gradient for free").
 */
void Convection_Diffusion_Magnetic_field::preparer_vitesse_aux_elems() const
{
  if (vitesse_aux_preparees_)
    return;
  // Act on the *effective* transport velocity: the base vitesse_transportante()
  // is the data placeholder in standalone mode and the Navier-Stokes unknown
  // once completer() has rebound it (two-way mode, vitesse_hydraulique).
  const Champ_Inc_base& vit = vitesse_transportante();
  if (sub_type(Champ_Face_PolyMAC_MPFA, vit))
    {
      Champ_Face_PolyMAC_MPFA& vit_m =
        ref_cast_non_const(Champ_Face_PolyMAC_MPFA, vit);
      vit_m.init_auxiliary_variables();
      // update_ve() gates on the MD_Vector of the *current* temporal case only;
      // drive it over every case so that passe()/futur() are all populated.
      for (int n = 0; n < vit_m.nb_valeurs_temporelles(); n++)
        vit_m.update_ve(vit_m.futur(n));
    }
  vitesse_aux_preparees_ = true;
}

/*! @brief Builds the kernel velocity gradient field `gradient_vitesse`
 *         (Discret_Thyd::grad_u) once.
 */
void Convection_Diffusion_Magnetic_field::construire_gradient_vitesse() const
{
  if (gradient_vitesse_)
    return;
  preparer_vitesse_aux_elems();
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, discretisation());
  dis.grad_u(domaine_dis(), domaine_Cl_dis(), vitesse_transportante(), gradient_vitesse_);
  gradient_vitesse_->mettre_a_jour(schema_temps().temps_init());
}

/*! @brief Fills `u_elem` (nb_elem x dim) with the velocity at the elements. */
void Convection_Diffusion_Magnetic_field::vitesse_aux_elements(DoubleTab& u_elem) const
{
  preparer_vitesse_aux_elems();

  const Domaine_VF& dvf = ref_cast(Domaine_VF, domaine_dis());
  const int ne = dvf.nb_elem();
  u_elem.resize(ne, dimension);
  IntVect polys(ne);
  for (int e = 0; e < ne; e++)
    polys(e) = e;
  // dvf.xp(): centre de gravite des elements (positionnement des valeurs).
  // The effective transport velocity (base vitesse_transportante()): the
  // data placeholder in standalone mode, the Navier-Stokes unknown in the
  // two-way mode — never the raw placeholder member.
  vitesse_transportante().valeur_aux_elems(dvf.xp(), polys, u_elem);
}

/*! @brief Re-extracts the element block of `gradient_vitesse` into
 *         `g` (nb_elem x dim*dim), row-major g(e, i*dim+j) = du_i/dx_j.
 *
 *  The kernel field stores the element gradient at row nb_faces_tot + e*dim+j,
 *  column i (i.e. the transpose of the natural per-element layout, as
 *  documented in KERNEL_REUSE_NOTES.md "Getting a velocity gradient for
 *  free", used by Terme_Source_Induction_stretching).
 */
void Convection_Diffusion_Magnetic_field::gradient_vitesse_elements(DoubleTab& g) const
{
  construire_gradient_vitesse();

  const DoubleTab& gu = gradient_vitesse_->valeurs();
  const Domaine_VF& dvf = ref_cast(Domaine_VF, domaine_dis());
  const int ne = dvf.nb_elem(), nf_tot = dvf.nb_faces_tot(), dim = Objet_U::dimension;
  g.resize(ne, dim * dim);
  // The kernel element gradient sits at row nf_tot + e*dim + j, column i
  // (transpose of the natural layout); re-extract as g(e, i*dim+j) = du_i/dx_j.
  for (int e = 0; e < ne; e++)
    for (int i = 0; i < dim; i++)
      for (int j = 0; j < dim; j++)
        g(e, i * dim + j) = gu(nf_tot + e * dim + j, i);
}


/*! @brief Green-Gauss element gradient of the magnetic unknown.
 *
 *  Two passes:
 *  - Pass 1 fills the standard Green-Gauss gradient for every element (local
 *    and ghost), taking the element's own value at boundary faces (zero
 *    normal gradient, consistent with the module's Neumann-like wall CLs);
 *  - Pass 2 replaces the gradient of wall-adjacent cells by that of a nearby
 *    interior cell found by BFS (the B2 correction), which is exact for
 *    linear fields and O(h) otherwise — Pass 1 alone is O(h) wrong at
 *    boundary cells.
 *
 *  @param[out] gb the gradient table, resized to
 *                 (nb_elem_tot, nb_composantes * dimension), row-major
 *                 gb(e, c*dim + d) = d b_c / d x_d.  Covers ALL elements so
 *                 parallel consumers may index up to nb_elem_tot().
 */
void Convection_Diffusion_Magnetic_field::gradient_b_elements(DoubleTab& gb) const
{
  const Domaine_VF& dvf = ref_cast(Domaine_VF, domaine_dis());
  const DoubleTab& b = couplage_fort() ? inconnue().futur() : inconnue().valeurs();
  // gb covers ALL elements, local and ghost: parallel consumers (the Lorentz
  // source) evaluate per-element data on ghost elements through joint faces,
  // so ghost rows must be filled with valid data, never read out of bounds.
  const int ne_tot = dvf.nb_elem_tot(), ne = dvf.nb_elem();
  const int nb_b = nb_composantes(), dim = Objet_U::dimension;
  gb.resize(ne_tot, nb_b * dim);
  gb = 0.;

  const IntTab& face_voisins = dvf.face_voisins();
  const IntTab& elem_faces = dvf.elem_faces();
  const DoubleTab& face_normales = dvf.face_normales();
  const DoubleVect& volumes = dvf.volumes();
  const int nfe = elem_faces.dimension(1);

  // Pass 1 — standard Green-Gauss (interior exact for linear fields), built
  // element-wise from elem_faces(): valid for every element up to ne_tot in
  // parallel, the same pattern as the kernel PolyMAC field operators.
  // Boundary faces take the element's own value (zero normal gradient).
  for (int e = 0; e < ne_tot; e++)
    {
      const double inv_ve = 1. / volumes(e);
      for (int j = 0; j < nfe; j++)
        {
          const int f = elem_faces(e, j);
          if (f < 0)
            continue;
          // Orientation: the face normal points out of face_voisins(f, 0).
          const bool is_e0 = (face_voisins(f, 0) == e);
          const int e_other = is_e0 ? face_voisins(f, 1) : face_voisins(f, 0);
          const double sgn = is_e0 ? 1. : -1.;
          for (int c = 0; c < nb_b; c++)
            {
              const double bf = (e_other < 0) ? b(e, c) : 0.5 * (b(e, c) + b(e_other, c));
              for (int d = 0; d < dim; d++)
                gb(e, c * dim + d) += sgn * bf * face_normales(f, d) * inv_ve;
            }
        }
    }

  // Pass 2 — boundary-cell correction (B2): replace the full gradient tensor
  // of boundary cells by the gradient of an interior neighbour, which for a
  // linear field is exact and O(h) for general nonlinear fields.  This gives
  // exact (linear-field) divergence at the two wall-adjacent cells, where
  // Pass 1 would have O(h) error.
  // Determine boundary/interior status based on cell adjacency to wall faces.
  // Local faces only: ghost elements are never wall-adjacent, and keeping the
  // neighbour search inside this rank's local cells makes it deterministic.
  const int nb_faces = dvf.nb_faces();
  std::vector<int> is_boundary(ne, 0);
  for (int f = 0; f < nb_faces; f++)
    {
      const int e0 = face_voisins(f, 0), e1 = face_voisins(f, 1);
      if (e0 >= 0 && e0 < ne && e1 < 0)
        is_boundary[e0] = 1;
      if (e1 >= 0 && e1 < ne && e0 < 0)
        is_boundary[e1] = 1;
    }

  // Build adjacency list for boundary cells.
  std::vector<std::vector<int>> adj(ne);
  for (int f = 0; f < nb_faces; f++)
    {
      const int e0 = face_voisins(f, 0), e1 = face_voisins(f, 1);
      if (e0 < 0 || e0 >= ne || e1 < 0 || e1 >= ne)
        continue;
      adj[e0].push_back(e1);
      adj[e1].push_back(e0);
    }

  // Find reference interior neighbour via BFS (depth cap 3) for each boundary cell.
  std::vector<int> ref(ne, -1);
  for (int e = 0; e < ne; e++)
    {
      if (!is_boundary[e])
        continue;
      // BFS outward to first fully-interior cell.
      std::vector<char> seen(ne, 0);
      seen[e] = 1;
      std::vector<int> frontier = {e};
      for (int depth = 0; depth < 3 && ref[e] < 0; depth++)
        {
          std::vector<int> nxt;
          for (int cur : frontier)
            {
              for (int n : adj[cur])
                {
                  if (seen[n])
                    continue;
                  seen[n] = 1;
                  nxt.push_back(n);
                  if (!is_boundary[n])
                    { ref[e] = n; break; }
                }
              if (ref[e] >= 0)
                break;
            }
          frontier = nxt;
        }
      // Fallback: nearest-adject neighbor.
      if (ref[e] < 0)
        for (int n : adj[e])
          { if (is_boundary[n] == 0) { ref[e] = n; break; } }
      if (ref[e] < 0)
        if (adj[e].size() > 0)
          ref[e] = adj[e][0];
    }

  // Apply correction: copy full tensor from interior reference (only if has ref).
  for (int e = 0; e < ne; e++)
    {
      if (ref[e] < 0)
        continue;
      const int r = ref[e];
      for (int k = 0; k < nb_b * dim; k++)
        gb(e, k) = gb(r, k);
    }
}
