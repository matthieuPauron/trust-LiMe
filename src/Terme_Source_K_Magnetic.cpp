#include <Terme_Source_K_Magnetic.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Convection_Diffusion_K_Magnetic.h>
#include <Convection_Diffusion_Epsilon_Magnetic.h>
#include <Modele_turbulence_magnetique.h>
#include <Probleme_base.h>
#include <Domaine_VF.h>
#include <Process.h>

Implemente_instanciable(Terme_Source_K_Magnetic,
                        "Terme_Source_K_Magnetic",
                        Source_base);

Sortie& Terme_Source_K_Magnetic::printOn(Sortie& os) const
{
  return Source_base::printOn(os);
}

Entree& Terme_Source_K_Magnetic::readOn(Entree& is)
{
  return Source_base::readOn(is);
}

void Terme_Source_K_Magnetic::associer_pb(const Probleme_base& pb)
{
  for (int i = 0; i < pb.nombre_d_equations(); i++)
    if (sub_type(Convection_Diffusion_Magnetic_field, pb.equation(i)))
      {
        equation_magnetique_ = ref_cast(Convection_Diffusion_Magnetic_field, pb.equation(i));
        if (const Modele_turbulence_magnetique* modele = equation_magnetique_->modele_turbulence_magnetique())
          modele_ = *modele;
        break;
      }
  if (!equation_magnetique_ || !modele_)
    Process::exit("Terme_Source_K_Magnetic requires a magnetic equation with an active magnetic turbulence model.");
}

void Terme_Source_K_Magnetic::ajouter_source(DoubleTab& secmem) const
{
  if (!equation_magnetique_ || !modele_)
    Process::exit("Terme_Source_K_Magnetic is not associated with the magnetic turbulence model.");

  DoubleTab production;
  modele_->calculer_production(production);
  const DoubleTab& eps = modele_->equation_epsilon().inconnue().valeurs();
  const Domaine_VF& domaine_vf = ref_cast(Domaine_VF, equation_magnetique_->domaine_dis());
  const DoubleVect& volumes = domaine_vf.volumes();
  const DoubleVect& porosite = equation_magnetique_->milieu().porosite_elem();
  const int ne = secmem.dimension(0);
  for (int e = 0; e < ne; e++)
    secmem(e, 0) += porosite(e) * volumes(e) * (production(e, 0) - eps(e, 0));
}

DoubleTab& Terme_Source_K_Magnetic::calculer(DoubleTab& secmem) const
{
  secmem = 0.;
  return ajouter(secmem);
}

DoubleTab& Terme_Source_K_Magnetic::ajouter(DoubleTab& secmem) const
{
  ajouter_source(secmem);
  return secmem;
}

void Terme_Source_K_Magnetic::ajouter_blocs(matrices_t, DoubleTab& secmem, const tabs_t&) const
{
  ajouter_source(secmem);
}
