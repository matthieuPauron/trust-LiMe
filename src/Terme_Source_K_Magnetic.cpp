#include <Terme_Source_K_Magnetic.h>

Implemente_instanciable(Terme_Source_K_Magnetic,
                        "Terme_Source_K_Magnetic",
                        Objet_U);

Sortie& Terme_Source_K_Magnetic::printOn(Sortie& os) const { return os; }
Entree& Terme_Source_K_Magnetic::readOn(Entree& is) { return is; }
