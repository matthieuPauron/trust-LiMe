/****************************************************************************
 * trust-LiMe — magnetic (induction) equation, standalone.
 * See Convection_Diffusion_Magnetic_field.h for the design notes.
 ****************************************************************************/

#include <Convection_Diffusion_Magnetic_field.h>
#include <Champ_Fonc_MHD.h>
#include <Probleme_base.h>
#include <Discret_Thyd.h>
#include <Param.h>
#include <Process.h>
#include <Champ_Inc_base.h>
#include <Champ_Don_base.h>
#include <Champ_Face_PolyMAC_MPFA.h>
#include <Fluide_base.h>
#include <Domaine_VF.h>
#include <Domaine_Cl_dis_base.h>

Implemente_instanciable(Convection_Diffusion_Magnetic_field,
                        "Convection_Diffusion_Magnetic_field",
                        Convection_Diffusion_Temperature_base);
// XD Convection_Diffusion_Magnetic_field eqn_base Convection_Diffusion_Magnetic_field INHERITS_BRACE
// XD_CONT Standalone convection-diffusion (induction) equation for the magnetic field b:
// XD_CONT d b / d t + (u . grad) b - div(eta grad b) = 0.
// XD attr diffusivite_magnetique field_base diffusivite_magnetique REQ Magnetic diffusivity eta = 1/(mu0 sigma).
// XD attr vitesse_magnetique field_base vitesse_magnetique OPT Transporting velocity u (default: zero).
// XD attr nb_composantes entier nb_composantes OPT Number of transported components of the magnetic field (default: the
// XD_CONT space dimension). Use 1 for a single magnetic component.
// XD attr mu0 floattant mu0 OPT Magnetic permeability mu_0, used by the post-treatment fields
// XD_CONT (energie_magnetique, vecteur_poynting). Default: 1.
// XD attr rapport_charge_masse floattant rapport_charge_masse OPT Charge-to-mass ratio q/m for the
// XD_CONT pulsation_cyclotronique post-treatment field. Default: e/m_e = 1.75882001076e11 C/kg
// XD_CONT (electron mass, positron charge).

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

  // The magnetic equation is a pure evolution equation: d b / d t.  Its time
  // term must therefore use the identity mass matrix, i.e. no coefficient
  // field.  In particular, the previous value "magnetic_field" was the
  // *unknown's own name*, which made Solveur_Masse_base::appliquer() divide the
  // mass term by b: the scheme became nonlinear and the measured diffusion
  // rate diverged from eta (and from the analytic exp(-eta k^2 t)).
  if (solveur_masse)
    solveur_masse->set_name_of_coefficient_temporel("no_coeff");

  return is;
}

/*! @brief Registers the magnetic keywords on top of the inherited ones. */
void Convection_Diffusion_Magnetic_field::set_param(Param& param) const
{
  Convection_Diffusion_Temperature_base::set_param(param);
  // NOTE: `diffusivite_magnetique` must appear before `diffusion` in the data
  // file (it is queried when the diffusion operator is read).
  param.ajouter("diffusivite_magnetique", &diffusivite_magnetique_);
  param.ajouter("vitesse_magnetique", &vitesse_magnetique_);
  // Constants consumed by the post-treatment fields (Champ_Fonc_MHD) only;
  // they have no influence on the transport itself.  See
  // docs/content/model.md ("Post-treatment fields") for the conventions.
  param.ajouter("mu0", &mu0_);
  param.ajouter("rapport_charge_masse", &rapport_charge_masse_);
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

const Champ_Don_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_transport() const
{
  if (!diffusivite_magnetique_)
    Process::exit("Convection_Diffusion_Magnetic_field: the keyword 'diffusivite_magnetique' "
                  "(magnetic diffusivity eta) is required and must be given before 'diffusion'.");
  return diffusivite_magnetique_.valeur();
}

const Champ_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_pas_de_temps() const
{
  return diffusivite_pour_transport();
}

const Champ_base& Convection_Diffusion_Magnetic_field::vitesse_pour_transport() const
{
  if (!la_vitesse_transportante_)
    Process::exit("Convection_Diffusion_Magnetic_field: transporting velocity not discretized.");
  return la_vitesse_transportante_.valeur();
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
  // Vector field and its components ("vecteur_poynting_x", ...).
  if (nom.debute_par("vecteur_poynting"))
    return &ch_vecteur_poynting_;
  return nullptr;
}

OWN_PTR(Champ_Fonc_base)* Convection_Diffusion_Magnetic_field::champ_postraitable(const Motcle& nom)
{
  // Owns and mutates the members, so this overload is used by creer_champ().
  const Convection_Diffusion_Magnetic_field& me = *this;
  return const_cast<OWN_PTR(Champ_Fonc_base)*>(me.champ_postraitable(nom));
}

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
  if (sub_type(Champ_Face_PolyMAC_MPFA, la_vitesse_transportante_.valeur()))
    {
      Champ_Face_PolyMAC_MPFA& vit =
        ref_cast_non_const(Champ_Face_PolyMAC_MPFA, la_vitesse_transportante_.valeur());
      vit.init_auxiliary_variables();
      // update_ve() gates on the MD_Vector of the *current* temporal case only;
      // drive it over every case so that passe()/futur() are all populated.
      for (int n = 0; n < vit.nb_valeurs_temporelles(); n++)
        vit.update_ve(vit.futur(n));
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
  la_vitesse_transportante_->valeur_aux_elems(dvf.xp(), polys, u_elem);
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

