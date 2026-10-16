#include <Paroi_isolante_magnetique.h>

Implemente_instanciable(Paroi_isolante_magnetique,
                        "paroi_isolante_magnetique",
                        Neumann_paroi_adiabatique);

Sortie& Paroi_isolante_magnetique::printOn(Sortie& s) const
{
  return s << que_suis_je() << finl;
}

Entree& Paroi_isolante_magnetique::readOn(Entree& s)
{
  if (app_domains.size() == 0)
    app_domains = { Motcle("Magnetique"), Motcle("Thermique"),
                    Motcle("indetermine")
                  };
  return Neumann_paroi_adiabatique::readOn(s);
}
