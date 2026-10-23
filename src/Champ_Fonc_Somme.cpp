/****************************************************************************
 * trust-LiMe — point-wise sum of two fields.
 ****************************************************************************/

#include <Champ_Fonc_Somme.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Champ_Uniforme.h>
#include <Champ_base.h>
#include <TRUSTTab.h>
#include <Param.h>

Implemente_instanciable(Champ_Fonc_Somme,
                        "Champ_Fonc_Somme",
                        Champ_Fonc_Elem_PolyMAC_CDO);
// XD Champ_Fonc_Somme champ_don_base Champ_Fonc_Somme BRACE Point-wise sum of two fields (eta_eff = eta + eta_t).

/*! @brief Binds the field to its domain and the two operands. */
void Champ_Fonc_Somme::configurer(const Convection_Diffusion_Magnetic_field& eqn,
                                  const Champ_base& a, const Champ_base& b)
{
  a_ = a;
  b_ = b;

  nommer("diffusivite_effective");
  associer_domaine_dis_base(eqn.domaine_dis());

  // Scalar (single-component) diffusivity: eta has one component.
  fixer_nature_du_champ(Nature_du_champ::scalaire);
  fixer_nb_comp(1);

  // Size the value table: one row per element, like Champ_Fonc_MHD.
  fixer_nb_valeurs_nodales(le_dom_VF->nb_elem());
}

/*! @brief Recomputes the sum from the operands, then forwards time bookkeeping. */
void Champ_Fonc_Somme::mettre_a_jour(double tps)
{
  // Refresh operands first (they may be Champ_Fonc themselves).
  if (a_)
    {
      Champ_Fonc_base* cf = dynamic_cast<Champ_Fonc_base*>(&a_.valeur());
      if (cf) cf->mettre_a_jour(tps);
    }
  if (b_)
    {
      Champ_Fonc_base* cf = dynamic_cast<Champ_Fonc_base*>(&b_.valeur());
      if (cf) cf->mettre_a_jour(tps);
    }
  calculer();
  Champ_Fonc_Elem_PolyMAC_CDO::mettre_a_jour(tps);
}

/*! @brief Fills valeurs() with a + b, broadcasting uniform operands. */
void Champ_Fonc_Somme::calculer()
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

Sortie& Champ_Fonc_Somme::printOn(Sortie& os) const
{
  return Champ_Fonc_Elem_PolyMAC_CDO::printOn(os);
}

Entree& Champ_Fonc_Somme::readOn(Entree& is)
{
  return Champ_Fonc_Elem_PolyMAC_CDO::readOn(is);
}
