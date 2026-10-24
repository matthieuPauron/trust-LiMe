#include <Convection_Diffusion_Epsilon_Magnetic.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Modele_turbulence_magnetique.h>
#include <Discret_Thyd.h>
#include <Probleme_base.h>
#include <Process.h>

Implemente_instanciable(Convection_Diffusion_Epsilon_Magnetic,
                        "Convection_Diffusion_Epsilon_Magnetic",
                        Convection_Diffusion_Temperature_base);
// XD Convection_Diffusion_Epsilon_Magnetic eqn_base Convection_Diffusion_Epsilon_Magnetic INHERITS_BRACE
// XD_CONT Transport equation for the magnetic-fluctuation dissipation epsilon_b.

Sortie& Convection_Diffusion_Epsilon_Magnetic::printOn(Sortie& os) const
{
  return Convection_Diffusion_Temperature_base::printOn(os);
}

Entree& Convection_Diffusion_Epsilon_Magnetic::readOn(Entree& is)
{
  if (!epsilon_b_)
    Process::exit("Convection_Diffusion_Epsilon_Magnetic: unknown was not discretized before readOn().");
  Convection_Diffusion_Temperature_base::readOn(is);
  if (solveur_masse)
    solveur_masse->set_name_of_coefficient_temporel("no_coeff");
  if (sources().est_vide())
    {
      Source src;
      Source& source = sources().add(src);
      source.typer_direct("Terme_Source_Epsilon_Magnetic");
      source->associer_eqn(*this);
      source->discretiser();
    }
  return is;
}

void Convection_Diffusion_Epsilon_Magnetic::discretiser()
{
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, discretisation());
  dis.concentration(schema_temps(), domaine_dis(), epsilon_b_, 1, "epsilon_b");
  epsilon_b_->nommer("epsilon_b");
  champs_compris_.ajoute_champ(epsilon_b_);
  Equation_base::discretiser();
}

void Convection_Diffusion_Epsilon_Magnetic::associer_milieu_base(const Milieu_base& mil)
{
  Convection_Diffusion_Temperature_base::associer_milieu_base(mil);
}

const Champ_Don_base& Convection_Diffusion_Epsilon_Magnetic::diffusivite_pour_transport() const
{
  if (modele_)
    return modele_->diffusivite_epsilon();
  const int n_eq = probleme().nombre_d_equations();
  for (int i = 0; i < n_eq; i++)
    if (sub_type(Convection_Diffusion_Magnetic_field, probleme().equation(i)))
      return ref_cast(Convection_Diffusion_Magnetic_field, probleme().equation(i)).diffusivite_pour_transport();
  Process::exit("Convection_Diffusion_Epsilon_Magnetic: no magnetic induction equation in the problem.");
  return Convection_Diffusion_Temperature_base::diffusivite_pour_transport();
}

const Champ_base& Convection_Diffusion_Epsilon_Magnetic::diffusivite_pour_pas_de_temps() const
{
  return diffusivite_pour_transport();
}

const Champ_base& Convection_Diffusion_Epsilon_Magnetic::vitesse_pour_transport() const
{
  const int n_eq = probleme().nombre_d_equations();
  for (int i = 0; i < n_eq; i++)
    if (sub_type(Convection_Diffusion_Magnetic_field, probleme().equation(i)))
      return ref_cast(Convection_Diffusion_Magnetic_field, probleme().equation(i)).vitesse_pour_transport();
  Process::exit("Convection_Diffusion_Epsilon_Magnetic: no magnetic induction equation in the problem.");
  return Convection_Diffusion_Temperature_base::vitesse_pour_transport();
}

void Convection_Diffusion_Epsilon_Magnetic::completer()
{
  if (!modele_)
    Process::exit("Convection_Diffusion_Epsilon_Magnetic requires Modele_turbulence_magnetique on the induction equation.");
  terme_diffusif.associer_diffusivite(modele_->diffusivite_epsilon());
  terme_diffusif.associer_diffusivite_pour_pas_de_temps(modele_->diffusivite_epsilon());
  const Champ_base& velocity = vitesse_pour_transport();
  associer_vitesse(velocity);
  terme_convectif.associer_vitesse(velocity);
  terme_convectif.valeur().associer_vitesse(velocity);
  Convection_Diffusion_Temperature_base::completer();
}

void Convection_Diffusion_Epsilon_Magnetic::mettre_a_jour(double temps)
{
  Convection_Diffusion_Temperature_base::mettre_a_jour(temps);
  if (modele_)
    modele_->controler_et_mettre_a_jour(temps);
}
