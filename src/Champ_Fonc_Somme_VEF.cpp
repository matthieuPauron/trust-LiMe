#include <Champ_Fonc_Somme_VEF.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Champ_Uniforme.h>
#include <Champ_base.h>
#include <TRUSTTab.h>
#include <Param.h>

Implemente_instanciable(Champ_Fonc_Somme_VEF,
                        "Champ_Fonc_Somme_VEF",
                        Champ_Fonc_P0_VEF);

void Champ_Fonc_Somme_VEF::configurer(const Convection_Diffusion_Magnetic_field& eqn,
                                      const Champ_base& a, const Champ_base& b)
{
  a_ = a;
  b_ = b;

  nommer("diffusivite_effective");
  associer_domaine_dis_base(eqn.domaine_dis());

  fixer_nature_du_champ(Nature_du_champ::scalaire);
  fixer_nb_comp(1);

  fixer_nb_valeurs_nodales(le_dom_VF.valeur().nb_elem());
}

void Champ_Fonc_Somme_VEF::mettre_a_jour(double tps)
{
  if (a_)
    {
      Champ_base& av = a_.valeur();
      Champ_Fonc_base* cf = dynamic_cast<Champ_Fonc_base*>(&av);
      if (cf) cf->mettre_a_jour(tps);
    }
  if (b_)
    {
      Champ_Fonc_base* cf = dynamic_cast<Champ_Fonc_base*>(&b_.valeur());
      if (cf) cf->mettre_a_jour(tps);
    }
  calculer();
  Champ_Fonc_P0_VEF::mettre_a_jour(tps);
}

void Champ_Fonc_Somme_VEF::calculer()
{
  if (!a_ || !b_)
    return;

  DoubleTab& ve = valeurs();
  const int ne = ve.dimension(0);
  const int nc = ve.dimension(1);

  const DoubleTab& va = a_->valeurs();
  const DoubleTab& vb = b_->valeurs();

  for (int e = 0; e < ne; e++)
    for (int c = 0; c < nc; c++)
      {
        double va_e = (va.dimension(0) == 1) ? va(0, c % va.dimension(1)) : va(e, c % va.dimension(1));
        double vb_e = (vb.dimension(0) == 1) ? vb(0, c % vb.dimension(1)) : vb(e, c % vb.dimension(1));
        ve(e, c) = va_e + vb_e;
      }
}

Sortie& Champ_Fonc_Somme_VEF::printOn(Sortie& os) const
{
  return Champ_Fonc_P0_VEF::printOn(os);
}

Entree& Champ_Fonc_Somme_VEF::readOn(Entree& is)
{
  return Champ_Fonc_P0_VEF::readOn(is);
}
