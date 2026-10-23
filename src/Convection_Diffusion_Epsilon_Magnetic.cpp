#include <Convection_Diffusion_Epsilon_Magnetic.h>

Implemente_instanciable(Convection_Diffusion_Epsilon_Magnetic,
                        "Convection_Diffusion_Epsilon_Magnetic",
                        Objet_U);

Sortie& Convection_Diffusion_Epsilon_Magnetic::printOn(Sortie& os) const { return os; }
Entree& Convection_Diffusion_Epsilon_Magnetic::readOn(Entree& is) { return is; }
