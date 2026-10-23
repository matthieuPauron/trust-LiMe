#include <Terme_Source_Epsilon_Magnetic.h>

Implemente_instanciable(Terme_Source_Epsilon_Magnetic,
                        "Terme_Source_Epsilon_Magnetic",
                        Objet_U);

Sortie& Terme_Source_Epsilon_Magnetic::printOn(Sortie& os) const { return os; }
Entree& Terme_Source_Epsilon_Magnetic::readOn(Entree& is) { return is; }
