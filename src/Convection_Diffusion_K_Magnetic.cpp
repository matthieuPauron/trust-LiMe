#include <Convection_Diffusion_K_Magnetic.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Modele_turbulence_magnetique.h>
#include <Discret_Thyd.h>
#include <Probleme_base.h>
#include <Process.h>

Implemente_instanciable(Convection_Diffusion_K_Magnetic,
                        "Convection_Diffusion_K_Magnetic",
                        Convection_Diffusion_Temperature_base);
// XD Convection_Diffusion_K_Magnetic eqn_base Convection_Diffusion_K_Magnetic INHERITS_BRACE
// XD_CONT Transport equation for the Alfvén-normalized magnetic fluctuation energy k_b.

Sortie& Convection_Diffusion_K_Magnetic::printOn(Sortie& os) const
{
  return Convection_Diffusion_Temperature_base::printOn(os);
}

Entree& Convection_Diffusion_K_Magnetic::readOn(Entree& is)
{
  if (!k_b_)
    Process::exit("Convection_Diffusion_K_Magnetic: unknown was not discretized before readOn().");
  Convection_Diffusion_Temperature_base::readOn(is);
  if (solveur_masse)
    solveur_masse->set_name_of_coefficient_temporel("no_coeff");
  if (sources().est_vide())
    {
      Source src;
      Source& source = sources().add(src);
      source.typer_direct("Terme_Source_K_Magnetic");
      source->associer_eqn(*this);
      source->discretiser();
    }
  return is;
}

void Convection_Diffusion_K_Magnetic::discretiser()
{
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, discretisation());
  dis.concentration(schema_temps(), domaine_dis(), k_b_, 1, "k_b");
  k_b_->nommer("k_b");
  champs_compris_.ajoute_champ(k_b_);
  Equation_base::discretiser();
}

void Convection_Diffusion_K_Magnetic::associer_milieu_base(const Milieu_base& mil)
{
  Convection_Diffusion_Temperature_base::associer_milieu_base(mil);
}

const Champ_Don_base& Convection_Diffusion_K_Magnetic::diffusivite_pour_transport() const
{
  if (modele_)
    return modele_->diffusivite_k();
  const int n_eq = probleme().nombre_d_equations();
  for (int i = 0; i < n_eq; i++)
    if (sub_type(Convection_Diffusion_Magnetic_field, probleme().equation(i)))
      return ref_cast(Convection_Diffusion_Magnetic_field, probleme().equation(i)).diffusivite_pour_transport();
  Process::exit("Convection_Diffusion_K_Magnetic: no magnetic induction equation in the problem.");
  return Convection_Diffusion_Temperature_base::diffusivite_pour_transport();
}

const Champ_base& Convection_Diffusion_K_Magnetic::diffusivite_pour_pas_de_temps() const
{
  return diffusivite_pour_transport();
}

const Champ_base& Convection_Diffusion_K_Magnetic::vitesse_pour_transport() const
{
  const int n_eq = probleme().nombre_d_equations();
  for (int i = 0; i < n_eq; i++)
    if (sub_type(Convection_Diffusion_Magnetic_field, probleme().equation(i)))
      return ref_cast(Convection_Diffusion_Magnetic_field, probleme().equation(i)).vitesse_pour_transport();
  Process::exit("Convection_Diffusion_K_Magnetic: no magnetic induction equation in the problem.");
  return Convection_Diffusion_Temperature_base::vitesse_pour_transport();
}

void Convection_Diffusion_K_Magnetic::completer()
{
  if (!modele_)
    Process::exit("Convection_Diffusion_K_Magnetic requires Modele_turbulence_magnetique on the induction equation.");
  const Champ_base& velocity = vitesse_pour_transport();
  associer_vitesse(velocity);
  terme_convectif.associer_vitesse(velocity);
  terme_convectif.valeur().associer_vitesse(velocity);
  Convection_Diffusion_Temperature_base::completer();

  // The base completer binds the molecular diffusivity; restore the closure's
  // eta + eta_t/sigma_k field on both the operator wrapper and its operator.
  terme_diffusif.associer_diffusivite(modele_->diffusivite_k());
  terme_diffusif.valeur().associer_diffusivite(modele_->diffusivite_k());
  terme_diffusif.associer_diffusivite_pour_pas_de_temps(modele_->diffusivite_k());
}

void Convection_Diffusion_K_Magnetic::mettre_a_jour(double temps)
{
  Convection_Diffusion_Temperature_base::mettre_a_jour(temps);
}
