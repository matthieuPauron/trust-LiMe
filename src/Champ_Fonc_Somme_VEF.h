#ifndef Champ_Fonc_Somme_VEF_included
#define Champ_Fonc_Somme_VEF_included

#include <Champ_Fonc_P0_VEF.h>
#include <TRUST_Ref.h>

class Champ_base;
class Convection_Diffusion_Magnetic_field;

class Champ_Fonc_Somme_VEF : public Champ_Fonc_P0_VEF
{
  Declare_instanciable(Champ_Fonc_Somme_VEF);

public:
  void configurer(const Convection_Diffusion_Magnetic_field& eqn,
                  const Champ_base& a, const Champ_base& b);
  void mettre_a_jour(double tps) override;

private:
  void calculer();

  OBS_PTR(Champ_base) a_;
  OBS_PTR(Champ_base) b_;
};

#endif
