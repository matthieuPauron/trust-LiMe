/*
 * Convection_Diffusion_Magnetic_field.cpp
 *
 *  Created on: Feb 5, 2026
 *      Author: matthieu
 */

#include "Convection_Diffusion_Magnetic_field.h"
#include <Pb_MHD.h>


Implemente_instanciable(Convection_Diffusion_Magnetic_field, "Convection_Diffusion_Magnetic_field", Convection_Diffusion_Temperature_base);

Sortie& Convection_Diffusion_Magnetic_field::printOn(Sortie& is) const { return Convection_Diffusion_Temperature_base::printOn(is); }

Entree& Convection_Diffusion_Magnetic_field::readOn(Entree& is)
{
  assert(the_magnetic_field.non_nul());
  assert(le_fluide.non_nul());
  Convection_diffusion_Temperature_base::readOn(is);

  Nom num=inconnue().le_nom();
  num.suffix("magnetic_field");
  Nom nom="Convection_";
  nom+=num;

  solveur_masse->set_name_of_coefficient_temporel("magnetic_field");
  return is;
}

void Convection_Diffusion_Magnetic_field::set_param(Param& param)
{
  // On va lire ici les paramètres du milieu
  Convection_Diffusion_Temperature_base::set_param(param);
}

int Convection_Diffusion_Magnetic_field::lire_motcle_non_standard(const	Mocle& un_mot, Entree& is)
{
  return Convection_Diffusion_Temperature_base::lire_motcle_non_standard(un_mot, is);
}

void Convection_Diffusion_Magnetic_field::associer_milieu_base(const Milieu_base& un_milieu)
{
  if (sub_type(Fluide_base,un_milieu)) associer_fluide(ref_cast(Fluide_base, un_milieu));
  else Process::exit(que_suis_je() + " : le fluide " + un_milieu.que_suis_je() + " n'est pas de type Fluide_base!");
}

void Convection_Diffusion_Magnetic_field::discretiser()
{
	// Lignes sur la discrétisation


  Equation_base::discretiser();
  Cerr << "Convection_Diffusion_Magnetic_field::discretiser() ok" << finl;
}

int Convection_Diffusion_Magnetic_field::preparer_calcul()
{
	return Equation_base::preparer_calcul();
}

const Motclef& Convection_Diffusion_Magnetic_field::domaine_application() const
{
	static Motcle domaine = "Magnetic";
	return domaine;
}

void Convection_Diffusion_Magnetic_field::assembler(Matrice_Morse& mat_morse, const DoubleTab& present, DoubleTab& secmem)
{
	//big stuff
}

void Convection_Diffusion_Magnetic_field::assembler_blocs(matrices_t matrices, DoubleTab& secmem, const tabs_t& semi_impl)
{
	//big stuff
}






