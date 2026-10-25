#include <Paroi_conductrice_magnetique.h>

Implemente_instanciable(Paroi_conductrice_magnetique,
                        "paroi_conductrice_magnetique",
                        Cond_lim_base);
// XD paroi_conductrice_magnetique condlim_base paroi_conductrice_magnetique INHERITS_BRACE
// XD_CONT Perfectly conducting magnetic wall: tangential components of b are
// XD_CONT clamped to Dirichlet 0, normal component keeps zero flux (Neumann).

Sortie& Paroi_conductrice_magnetique::printOn(Sortie& s) const
{
  return s << que_suis_je() << finl;
}

Entree& Paroi_conductrice_magnetique::readOn(Entree& s)
{
  if (app_domains.size() == 0)
    app_domains = { Motcle("Magnetique"), Motcle("Thermique"),
                    Motcle("indetermine")
                  };
  le_champ_front.typer("Champ_front_uniforme");
  return s;
}
