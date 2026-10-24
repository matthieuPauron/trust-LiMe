/****************************************************************************
 * trust-LiMe — k_b source: P_b - epsilon_b.
 ****************************************************************************/

#ifndef Terme_Source_K_Magnetic_included
#define Terme_Source_K_Magnetic_included

#include <Source_base.h>
#include <TRUST_Ref.h>

class Convection_Diffusion_Magnetic_field;
class Modele_turbulence_magnetique;
class Probleme_base;

class Terme_Source_K_Magnetic : public Source_base
{
  Declare_instanciable(Terme_Source_K_Magnetic);

public:
  int has_interface_blocs() const override { return 1; }
  void associer_pb(const Probleme_base& pb) override;
  void mettre_a_jour(double) override { }
  void check_multiphase_compatibility() const override { }
  DoubleTab& calculer(DoubleTab& secmem) const override;
  DoubleTab& ajouter(DoubleTab& secmem) const override;
  void dimensionner_blocs(matrices_t, const tabs_t& = {}) const override { }
  void ajouter_blocs(matrices_t, DoubleTab& secmem, const tabs_t& = {}) const override;

protected:
  void set_param(Param&) const override { }
  void associer_domaines(const Domaine_dis_base&, const Domaine_Cl_dis_base&) override { }
  void ajouter_source(DoubleTab& secmem) const;

  OBS_PTR(Convection_Diffusion_Magnetic_field) equation_magnetique_;
  OBS_PTR(Modele_turbulence_magnetique) modele_;
};

#endif
