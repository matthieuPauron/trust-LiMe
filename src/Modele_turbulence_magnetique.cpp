#include <Modele_turbulence_magnetique.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Convection_Diffusion_K_Magnetic.h>
#include <Convection_Diffusion_Epsilon_Magnetic.h>
#include <Champ_Fonc_Eta_Turbulent.h>
#include <Champ_Fonc_Somme.h>
#include <Champ_Uniforme.h>
#include <Champ_Don_base.h>
#include <Champ_Inc_base.h>
#include <Domaine_VF.h>
#include <Probleme_base.h>
#include <Param.h>
#include <Process.h>
#include <algorithm>
#include <cmath>

Implemente_instanciable(Modele_turbulence_magnetique,
                        "Modele_turbulence_magnetique",
                        Objet_U);

Sortie& Modele_turbulence_magnetique::printOn(Sortie& os) const
{
  return os << que_suis_je();
}

Entree& Modele_turbulence_magnetique::readOn(Entree& is)
{
  Param param(que_suis_je());
  set_param(param);
  param.lire_avec_accolades(is);
  return is;
}

void Modele_turbulence_magnetique::set_param(Param& param) const
{
  param.ajouter("C_mu_b", &C_mu_b_);
  param.ajouter("C_eps1b", &C_eps1_);
  param.ajouter("C_eps2b", &C_eps2_);
  param.ajouter("sigma_kb", &sigma_kb_);
  param.ajouter("sigma_epsilonb", &sigma_epsilonb_);
  param.ajouter("prandtl_turbulent_magnetique_b", &Pm_t_b_);
  param.ajouter("k_b_min", &k_min_);
  param.ajouter("epsilon_b_min", &epsilon_min_);
  param.ajouter("eta_t_max", &eta_t_max_);
  param.ajouter("rho_ref", &rho_ref_);
}

void Modele_turbulence_magnetique::completer(const Convection_Diffusion_Magnetic_field& eqn)
{
  if (C_mu_b_ <= 0. || C_eps1_ <= 0. || C_eps2_ <= 0. || sigma_kb_ <= 0.
      || sigma_epsilonb_ <= 0. || Pm_t_b_ <= 0. || k_min_ <= 0.
      || epsilon_min_ <= 0. || eta_t_max_ <= 0.)
    Process::exit("Modele_turbulence_magnetique: closure coefficients and floors must be positive.");

  equation_magnetique_ = eqn;
  const Probleme_base& pb = eqn.probleme();
  for (int i = 0; i < pb.nombre_d_equations(); i++)
    {
      const Equation_base& e = pb.equation(i);
      if (sub_type(Convection_Diffusion_K_Magnetic, e))
        {
          if (equation_k_)
            Process::exit("Modele_turbulence_magnetique: only one Convection_Diffusion_K_Magnetic equation is allowed.");
          equation_k_ = ref_cast(Convection_Diffusion_K_Magnetic, e);
        }
      if (sub_type(Convection_Diffusion_Epsilon_Magnetic, e))
        {
          if (equation_epsilon_)
            Process::exit("Modele_turbulence_magnetique: only one Convection_Diffusion_Epsilon_Magnetic equation is allowed.");
          equation_epsilon_ = ref_cast(Convection_Diffusion_Epsilon_Magnetic, e);
        }
    }
  if (!equation_k_ || !equation_epsilon_)
    Process::exit("Modele_turbulence_magnetique requires exactly one k_b and one epsilon_b transport equation in the problem.");

  if (rho_ref_ <= 0.)
    {
      const Champ_Uniforme* rho = dynamic_cast<const Champ_Uniforme*>(&eqn.milieu().masse_volumique());
      if (!rho)
        Process::exit("Modele_turbulence_magnetique: rho is not Champ_Uniforme.");
      if (rho->valeurs().dimension(0) != 1 || rho->valeurs().dimension(1) != 1)
        Process::exit("Modele_turbulence_magnetique currently requires constant rho.");
      rho_ref_ = rho->valeurs()(0, 0);
    }
  if (rho_ref_ <= 0. || eqn.mu0() <= 0.)
    Process::exit("Modele_turbulence_magnetique requires positive rho_ref and mu0.");
  field_scale_ = 1. / std::sqrt(eqn.mu0() * rho_ref_);

  const Champ_base& k = equation_k_->inconnue();
  const Champ_base& eps = equation_epsilon_->inconnue();

  eta_t_.typer("Champ_Fonc_Eta_Turbulent");
  ref_cast(Champ_Fonc_Eta_Turbulent, eta_t_.valeur()).configurer(
    eqn, k, 0, eps, 0, C_mu_b_, Pm_t_b_, false, epsilon_min_, eta_t_max_);

  eta_t_k_.typer("Champ_Fonc_Eta_Turbulent");
  ref_cast(Champ_Fonc_Eta_Turbulent, eta_t_k_.valeur()).configurer(
    eqn, k, 0, eps, 0, C_mu_b_, Pm_t_b_ * sigma_kb_, false, epsilon_min_, eta_t_max_);
  eta_t_epsilon_.typer("Champ_Fonc_Eta_Turbulent");
  ref_cast(Champ_Fonc_Eta_Turbulent, eta_t_epsilon_.valeur()).configurer(
    eqn, k, 0, eps, 0, C_mu_b_, Pm_t_b_ * sigma_epsilonb_, false, epsilon_min_, eta_t_max_);

  diffusivite_k_.typer("Champ_Fonc_Somme");
  ref_cast(Champ_Fonc_Somme, diffusivite_k_.valeur()).configurer(
    eqn, eqn.diffusivite_pour_transport(), eta_t_k_.valeur());
  diffusivite_epsilon_.typer("Champ_Fonc_Somme");
  ref_cast(Champ_Fonc_Somme, diffusivite_epsilon_.valeur()).configurer(
    eqn, eqn.diffusivite_pour_transport(), eta_t_epsilon_.valeur());

  ref_cast(Convection_Diffusion_K_Magnetic, equation_k_.valeur()).associer_modele(*this);
  ref_cast(Convection_Diffusion_Epsilon_Magnetic, equation_epsilon_.valeur()).associer_modele(*this);
  mettre_a_jour(eqn.schema_temps().temps_courant());
}

void Modele_turbulence_magnetique::mettre_a_jour(double temps)
{
  if (eta_t_)
    {
      eta_t_->mettre_a_jour(temps);
    }
  if (diffusivite_k_)
    {
      diffusivite_k_->mettre_a_jour(temps);
    }
  if (diffusivite_epsilon_)
    {
      diffusivite_epsilon_->mettre_a_jour(temps);
    }
  // eta_eff (diffusivite_effective_) is updated by the problem-level update
  // via champs_compris_; do not touch it here.
}

void Modele_turbulence_magnetique::limiter(Champ_Inc_base& champ, int& count) const
{
  DoubleTab& values = champ.valeurs();
  count = 0;
  for (int e = 0; e < values.dimension(0); e++)
    {
      double& value = values(e, 0);
      const double floor = (&champ == &equation_k_->inconnue()) ? 0. : epsilon_min_;
      if (!std::isfinite(value))
        Process::exit("Modele_turbulence_magnetique: non-finite k_b/epsilon_b value.");
      if (value < floor)
        {
          value = floor;
          count++;
        }
    }
  values.echange_espace_virtuel();
}

void Modele_turbulence_magnetique::controler_et_mettre_a_jour(double temps)
{
  int count_k = 0, count_epsilon = 0;
  limiter(ref_cast_non_const(Champ_Inc_base, equation_k_->inconnue()), count_k);
  limiter(ref_cast_non_const(Champ_Inc_base, equation_epsilon_->inconnue()), count_epsilon);
  if (count_k || count_epsilon)
    Cerr << "Modele_turbulence_magnetique: positivity limiter changed " << count_k
         << " k_b and " << count_epsilon << " epsilon_b values at t=" << temps << finl;
  mettre_a_jour(temps);
}

void Modele_turbulence_magnetique::calculer_production(DoubleTab& production) const
{
  if (!equation_magnetique_ || !eta_t_)
    Process::exit("Modele_turbulence_magnetique: production requested before completer().");
  const Convection_Diffusion_Magnetic_field& eq = equation_magnetique_.valeur();
  DoubleTab grad_b;
  eq.gradient_b_elements(grad_b);
  const DoubleTab& eta_values = eta_t_->valeurs();
  const int ne = eta_values.dimension(0);
  production.resize(ne, 1);
  production = 0.;
  const int dim = Objet_U::dimension;
  const int nb_b = eq.nb_composantes();
  for (int e = 0; e < ne; e++)
    {
      double curl2 = 0.;
      if (dim == 2)
        {
          const double dby_dx = (nb_b > 1) ? grad_b(e, dim) : 0.;
          const double dbx_dy = grad_b(e, 1);
          const double curl_z = dby_dx - dbx_dy;
          curl2 = curl_z * curl_z;
        }
      else if (dim == 3)
        {
          const double dbz_dy = (nb_b > 2) ? grad_b(e, 2 * dim + 1) : 0.;
          const double dby_dz = (nb_b > 1) ? grad_b(e, dim + 2) : 0.;
          const double dbx_dz = grad_b(e, 2);
          const double dbz_dx = (nb_b > 2) ? grad_b(e, 2 * dim) : 0.;
          const double dby_dx = (nb_b > 1) ? grad_b(e, dim) : 0.;
          const double dbx_dy = grad_b(e, 1);
          const double curl_x = dbz_dy - dby_dz;
          const double curl_y = dbx_dz - dbz_dx;
          const double curl_z = dby_dx - dbx_dy;
          curl2 = curl_x * curl_x + curl_y * curl_y + curl_z * curl_z;
        }
      production(e, 0) = eta_values(e, 0) * field_scale_ * field_scale_ * curl2;
    }
}

const Champ_Don_base& Modele_turbulence_magnetique::eta_t() const
{
  return ref_cast(Champ_Don_base, eta_t_.valeur());
}
Champ_Don_base& Modele_turbulence_magnetique::eta_t()
{
  return ref_cast(Champ_Don_base, eta_t_.valeur());
}

const Champ_Don_base& Modele_turbulence_magnetique::diffusivite_k() const
{
  return ref_cast(Champ_Don_base, diffusivite_k_.valeur());
}
Champ_Don_base& Modele_turbulence_magnetique::diffusivite_k()
{
  return ref_cast(Champ_Don_base, diffusivite_k_.valeur());
}

const Champ_Don_base& Modele_turbulence_magnetique::diffusivite_epsilon() const
{
  return ref_cast(Champ_Don_base, diffusivite_epsilon_.valeur());
}
Champ_Don_base& Modele_turbulence_magnetique::diffusivite_epsilon()
{
  return ref_cast(Champ_Don_base, diffusivite_epsilon_.valeur());
}

const Convection_Diffusion_K_Magnetic& Modele_turbulence_magnetique::equation_k() const
{
  return equation_k_.valeur();
}

const Convection_Diffusion_Epsilon_Magnetic& Modele_turbulence_magnetique::equation_epsilon() const
{
  return equation_epsilon_.valeur();
}
