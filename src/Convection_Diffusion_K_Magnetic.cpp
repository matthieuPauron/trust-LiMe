#include <Convection_Diffusion_K_Magnetic.h>

Implemente_instanciable(Convection_Diffusion_K_Magnetic,
                        "Convection_Diffusion_K_Magnetic",
                        Objet_U);

Sortie& Convection_Diffusion_K_Magnetic::printOn(Sortie& os) const { return os; }
Entree& Convection_Diffusion_K_Magnetic::readOn(Entree& is) { return is; }
