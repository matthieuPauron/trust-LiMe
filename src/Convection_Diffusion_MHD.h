#ifndef Convection_Diffusion_MHD_included
#define Convection_Diffusion_MHD_included

#include <Convection_Diffusion_Temperature

/*! @brief classe Convection_Diffusion_MHD Convection diffusion equation for the magnetic field
 */

class Convection_Diffusion_MHD : public Convection_Diffusion_Temperature {
	Declare_base(Convection_Diffusion_MHD);

public:
	void set_param(Param &titi) override;
	int lire_motcle_non_standard(const Motcle&, Entree&) override;
	int nombre_d_operateurs() const override;
	const Operateur& operateur(int) const override;
	Operateur& operateur(int) override;

	const Champ_Inc_base& inconnue() const override { return magnetic_field_ };
	Champ_Inc_base &inconnue() override { return magnetic_field_ } ;
	
	
protected:
	OWN_PTR(Champ_Inc_base) magnetic_field_;	
}



#endif
