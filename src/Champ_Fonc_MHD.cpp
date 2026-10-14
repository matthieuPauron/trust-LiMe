/****************************************************************************
 * trust-LiMe — derived (post-treatment) fields of the magnetic equation.
 * See Champ_Fonc_MHD.h for the design notes.
 ****************************************************************************/

#include <Champ_Fonc_MHD.h>
#include <MHD_posttraitement_kernel.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Champ_Inc_base.h>
#include <Champ_Don_base.h>
#include <Domaine_dis_base.h>
#include <Domaine_VF.h>
#include <Process.h>
#include <cmath>

Implemente_instanciable(Champ_Fonc_MHD, "Champ_Fonc_MHD", Champ_Fonc_Elem_PolyMAC_CDO);

Sortie& Champ_Fonc_MHD::printOn(Sortie& s) const
{
  return s << que_suis_je() << " " << le_nom();
}

Entree& Champ_Fonc_MHD::readOn(Entree& s)
{
  // Programmatic construction only (see the equation's creer_champ()): the
  // field is never typed from the data file, so nothing to read.
  return s;
}

void Champ_Fonc_MHD::configurer(const Convection_Diffusion_Magnetic_field& eqn, const Motcle& kind)
{
  eqn_ = eqn;
  kind_ = kind;

  const int dim = Objet_U::dimension;
  const bool vectoriel = (kind == "vecteur_poynting");

  nommer(kind);
  associer_domaine_dis_base(eqn.domaine_dis());
  fixer_nature_du_champ(vectoriel ? Nature_du_champ::vectoriel : Nature_du_champ::scalaire);
  fixer_nb_comp(vectoriel ? dim : 1);

  // Per-component names ("pulsation_cyclotronique", or "vecteur_poynting_X/Y/Z").
  static const char* const dir[3] = { "X", "Y", "Z" };
  if (vectoriel)
    for (int d = 0; d < dim; d++)
      fixer_nom_compo(d, Nom(kind) + Nom("_") + Nom(dir[d]));
  else
    fixer_nom_compo(0, Nom(kind));

  fixer_nb_valeurs_nodales(le_dom_VF->nb_elem());
  // Units are informative only (TRUST fields are in the user's units).
  if (kind == "pulsation_cyclotronique")
    fixer_unite("s-1");
  else if (kind == "energie_magnetique")
    fixer_unite("J/m3");
  else if (kind == "vecteur_poynting")
    fixer_unite("W/m2");
  else
    fixer_unite("-");      // dimensionless / user's units
  changer_temps(-1.);  // force the first mettre_a_jour() to compute
}

void Champ_Fonc_MHD::mettre_a_jour(double tps)
{
  if (temps() != tps)
    calculer();
  Champ_Fonc_P0_base::mettre_a_jour(tps);
}

/*! @brief Fills valeurs() from the magnetic field b, the (frozen) velocity u
 *         and its element gradient, according to kind_.
 *
 *  All the per-element physics is factored into MHD_posttraitement_kernel.h.
 *  The diffusivity eta and the kinematic viscosity nu may be uniform or
 *  per-element fields — both layouts are accepted.
 */
void Champ_Fonc_MHD::calculer()
{
  const Convection_Diffusion_Magnetic_field& eq = eqn_.valeur();
  const DoubleTab& b = eq.inconnue().valeurs();        // (nb_elem, nb_composantes)
  DoubleTab& vals = valeurs();
  const int ne = vals.dimension(0);
  const int nb_b = eq.nb_composantes();
  const int dim = Objet_U::dimension;

  // ------ inputs prepared on demand ---------------------------------------
  const bool besoin_u = (kind_ == "vecteur_poynting" || kind_ == "helicite_croisee" ||
                         kind_ == "helicite_cinetique" || kind_ == "nombre_de_reynolds_magnetique");
  if (besoin_u)
    eq.vitesse_aux_elements(u_elem_);

  if (kind_ == "helicite_cinetique")
    eq.gradient_vitesse_elements(gu_elem_);

  // Per-element diffusivity / viscosity support both (1,1) and (ne,1) layouts
  const DoubleTab& tab_eta = eq.diffusivite_pour_transport().valeurs();
  const DoubleTab& tab_nu = eq.fluide().viscosite_cinematique().valeurs();
  const auto coeff_elem = [](const DoubleTab& t, int e) -> double
  { return t.dimension(0) == 1 ? t(0, 0) : t(e, 0); };

  const double mu0 = eq.mu0(), qm = eq.rapport_charge_masse();
  const Domaine_VF& dvf = ref_cast(Domaine_VF, eq.domaine_dis());
  const DoubleVect& vol = dvf.volumes();
  double w[3] = { 0., 0., 0. }, s[3] = { 0., 0., 0. };

  if (kind_ == "pulsation_cyclotronique")
    {
      for (int e = 0; e < ne; e++)
        vals(e, 0) = mhd_pulsation_cyclotronique(mhd_norme2_b(&b(e, 0), nb_b), qm);
    }
  else if (kind_ == "energie_magnetique")
    {
      for (int e = 0; e < ne; e++)
        vals(e, 0) = mhd_energie_magnetique(mhd_norme2_b(&b(e, 0), nb_b), mu0);
    }
  else if (kind_ == "vecteur_poynting")
    {
      for (int e = 0; e < ne; e++)
        {
          mhd_vecteur_poynting(&u_elem_(e, 0), &b(e, 0), nb_b, dim, mu0, s);
          for (int d = 0; d < dim; d++)
            vals(e, d) = s[d];
        }
    }
  else if (kind_ == "helicite_croisee")
    {
      for (int e = 0; e < ne; e++)
        vals(e, 0) = mhd_produit_scalar_u_b(&u_elem_(e, 0), &b(e, 0), nb_b, dim);
    }
  else if (kind_ == "helicite_cinetique")
    {
      for (int e = 0; e < ne; e++)
        {
          mhd_curl_u_elem(&gu_elem_(e, 0), dim, w);
          vals(e, 0) = mhd_helicite_cinetique(&u_elem_(e, 0), w, dim);
        }
    }
  else if (kind_ == "nombre_de_reynolds_magnetique")
    {
      for (int e = 0; e < ne; e++)
        {
          const double taille = std::pow(vol(e), 1. / dim);
          vals(e, 0) = mhd_reynolds_magnetique(&u_elem_(e, 0), dim, taille, coeff_elem(tab_eta, e));
        }
    }
  else if (kind_ == "nombre_de_prandtl_magnetique")
    {
      for (int e = 0; e < ne; e++)
        vals(e, 0) = mhd_prandtl_magnetique(coeff_elem(tab_nu, e), coeff_elem(tab_eta, e));
    }
  else
    {
      Cerr << "Error in Champ_Fonc_MHD::calculer: unknown post-treatment field kind '" << kind_
           << "' (expected energie_magnetique, pulsation_cyclotronique, vecteur_poynting, "
           << "helicite_croisee, helicite_cinetique, nombre_de_reynolds_magnetique "
           << "or nombre_de_prandtl_magnetique)." << finl;
      Process::exit();
    }

  vals.echange_espace_virtuel();
}
