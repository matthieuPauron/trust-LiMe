#include <Modele_turbulence_magnetique.h>
#include <Champ_Fonc_base.h>

Implemente_instanciable(Modele_turbulence_magnetique,
                        "Modele_turbulence_magnetique",
                        Objet_U);

Sortie& Modele_turbulence_magnetique::printOn(Sortie& os) const { return os; }
Entree& Modele_turbulence_magnetique::readOn(Entree& is) { return is; }
