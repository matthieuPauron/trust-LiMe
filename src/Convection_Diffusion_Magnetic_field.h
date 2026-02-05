#ifndef Convection_Diffusion_Magnetic_field_included
#define Convection_Diffusion_Magnetic_field_included

#include <Convection_Diffusion_Temperature>

/*! @brief classe Convection_Diffusion_MHD Convection diffusion equation for the magnetic field
 */

class Convection_Diffusion_Magnetic_field : public Convection_Diffusion_Temperature_base
{
  Declare_base(Convection_Diffusion_Magnetic_field);

public:
  void set_param(Param& titi) override;
  int lire_motcle_non_standard(const Motcle&, Entree&) override;
  const Champ_Inc_base& inconnue() const override { return the_magnetic_field_ };
  Champ_Inc_base& inconnue() override { return the_magnetic_field_ } ;
  void discretiser() override;
  int preparer_calcul() override;
  void associer_milieu_base(const Milieu_base& ) override;

  const Motcle& domaine_application() const override;

  void assembler( Matrice_Morse& mat_morse, const DoubleTab& present, DoubleTab& secmem) override ;
  void assembler_blocs(matrices_t matrices, DoubleTab& secmem, const tabs_t& semi_impl = {}) const override;

protected:
  OWN_PTR(Champ_Inc_base) the_magnetic_field_;
}



#endif
