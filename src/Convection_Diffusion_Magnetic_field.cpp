/****************************************************************************
 * trust-LiMe — magnetic (induction) equation, standalone.
 * See Convection_Diffusion_Magnetic_field.h for the design notes.
 ****************************************************************************/

#include <Convection_Diffusion_Magnetic_field.h>
#include <Probleme_base.h>
#include <Discret_Thyd.h>
#include <Param.h>
#include <Process.h>
#include <Champ_Inc_base.h>
#include <Champ_Don_base.h>

Implemente_instanciable(Convection_Diffusion_Magnetic_field,
                        "Convection_Diffusion_Magnetic_field",
                        Convection_Diffusion_Temperature_base);
// XD Convection_Diffusion_Magnetic_field eqn_base Convection_Diffusion_Magnetic_field INHERITS_BRACE
// XD_CONT Standalone convection-diffusion (induction) equation for the magnetic field b:
// XD_CONT d b / d t + (u . grad) b - div(eta grad b) = 0.
// XD attr diffusivite_magnetique field_base diffusivite_magnetique REQ Magnetic diffusivity eta = 1/(mu0 sigma).
// XD attr vitesse_magnetique field_base vitesse_magnetique OPT Transporting velocity u (default: zero).
// XD attr nb_composantes entier nb_composantes OPT Number of transported components of the magnetic field (default: the
// XD_CONT space dimension). Use 1 for a single magnetic component.

Sortie& Convection_Diffusion_Magnetic_field::printOn(Sortie& os) const
{
  return Convection_Diffusion_Temperature_base::printOn(os);
}

/*! @brief Reads the magnetic equation data block.
 *
 *  Reuses the kernel reading machinery: after the inherited read (which reads
 *  diffusion/convection, sources, boundary and initial conditions), the
 *  transporting velocity is filled from the optional data field, and the mass
 *  solver coefficient is renamed.
 */
Entree& Convection_Diffusion_Magnetic_field::readOn(Entree& is)
{
  assert(the_magnetic_field_);
  Convection_Diffusion_Temperature_base::readOn(is);

  // The convection operator was associated with la_vitesse_transportante_
  // (a Champ_Inc) while the block was read; fill it now from the data field.
  if (vitesse_magnetique_)
    la_vitesse_transportante_->affecter(vitesse_magnetique_.valeur());

  if (solveur_masse)
    solveur_masse->set_name_of_coefficient_temporel("magnetic_field");

  return is;
}

/*! @brief Registers the magnetic keywords on top of the inherited ones. */
void Convection_Diffusion_Magnetic_field::set_param(Param& param) const
{
  Convection_Diffusion_Temperature_base::set_param(param);
  // NOTE: `diffusivite_magnetique` must appear before `diffusion` in the data
  // file (it is queried when the diffusion operator is read).
  param.ajouter("diffusivite_magnetique", &diffusivite_magnetique_);
  param.ajouter("vitesse_magnetique", &vitesse_magnetique_);
  param.ajouter("nb_composantes", &nb_composantes_);
}

/*! @brief Builds the unknown and the transporting velocity fields.
 *
 *  The unknown is a cell-centred multi-component field (one component per
 *  space direction).  This mirrors Convection_Diffusion_Concentration, which
 *  transports several scalars with the same operators; no new discretisation
 *  is introduced.
 */
void Convection_Diffusion_Magnetic_field::discretiser()
{
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, discretisation());
  Cerr << "Magnetic field equation discretization" << finl;

  // Number of transported components: full vector by default.
  if (nb_composantes_ < 0)
    nb_composantes_ = Objet_U::dimension;
  if (nb_composantes_ < 1 || nb_composantes_ > Objet_U::dimension)
    Process::exit("Convection_Diffusion_Magnetic_field: 'nb_composantes' must be between 1 and the space "
                  "dimension.");

  // Magnetic field: one independent component per transported direction.
  dis.concentration(schema_temps(), domaine_dis(), the_magnetic_field_,
                    nb_composantes_, "magnetic_field");
  champs_compris_.ajoute_champ(the_magnetic_field_);

  // Transporting velocity: a vector field, zero unless overridden by data.
  dis.vitesse(schema_temps(), domaine_dis(), la_vitesse_transportante_, 1);
  la_vitesse_transportante_->nommer("vitesse_magnetique");

  Equation_base::discretiser();

  Cerr << "Convection_Diffusion_Magnetic_field::discretiser() ok" << finl;
}

const Champ_Don_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_transport() const
{
  if (!diffusivite_magnetique_)
    Process::exit("Convection_Diffusion_Magnetic_field: the keyword 'diffusivite_magnetique' "
                  "(magnetic diffusivity eta) is required and must be given before 'diffusion'.");
  return diffusivite_magnetique_.valeur();
}

const Champ_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_pas_de_temps() const
{
  return diffusivite_pour_transport();
}

const Champ_base& Convection_Diffusion_Magnetic_field::vitesse_pour_transport() const
{
  if (!la_vitesse_transportante_)
    Process::exit("Convection_Diffusion_Magnetic_field: transporting velocity not discretized.");
  return la_vitesse_transportante_.valeur();
}

/*! @brief Reuses the kernel scalar (thermal) boundary conditions.
 *
 *  Reporting "Thermique" makes the standard scalar CLs compatible
 *  (Cond_lim_base::compatible_avec_eqn).  Dedicated magnetic CLs are deferred.
 */
const Motcle& Convection_Diffusion_Magnetic_field::domaine_application() const
{
  static Motcle domaine = "Thermique";
  return domaine;
}
