/****************************************************************************
 * trust-LiMe — derived (post-treatment) fields of the magnetic equation.
 *
 * Champ_Fonc_MHD is the single Champ_Fonc element field class used by
 * Convection_Diffusion_Magnetic_field to expose the MHD post-treatment
 * fields (local cyclotron pulsation, magnetic energy, Poynting vector,
 * crossed/kinetic helicity, magnetic Reynolds and Prandtl numbers, magnetic
 * tension). One instance is created per requested name (equation creer_champ
 * hook), the Motcle kind_ selects the formula, and mettre_a_jour()
 * recomputes the values from the current magnetic field (and frozen
 * velocity) of the equation. All the physics conventions live in the
 * unit-testable, framework-free kernel MHD_posttraitement_kernel.h.
 *
 * For fields needing an element gradient of b (currently magnetic tension),
 * the equation's gradient_b_elements() provides it via a Green-Gauss
 * reconstruction — there is no PolyMAC element-field gradient factory in
 * the kernel (Discret_Thyd::grad_T() is a stub on this discretisation, the
 * generic Gradient post-processor rejects PolyMAC element fields), so the
 * module reconstructs it itself.
 *
 * The class derives from Champ_Fonc_Elem_PolyMAC_CDO — the canonical P0
 * element storage of the PolyMAC family — so the field is element-localised,
 * like the magnetic unknown itself.
 ****************************************************************************/

#ifndef Champ_Fonc_MHD_included
#define Champ_Fonc_MHD_included

#include <Champ_Fonc_Elem_PolyMAC_CDO.h>
#include <Motcle.h>
#include <TRUST_Ref.h>

class Convection_Diffusion_Magnetic_field;

/*! @brief Element field computed on demand from the magnetic equation.
 *
 *  @sa Champ_Fonc_Elem_PolyMAC_CDO, Convection_Diffusion_Magnetic_field
 */
class Champ_Fonc_MHD : public Champ_Fonc_Elem_PolyMAC_CDO
{
  Declare_instanciable(Champ_Fonc_MHD);

public:

  /*! @brief Binds the field to its equation and selects the computed quantity.
   *
   *  Also prepares the auxiliary machinery the selected quantity needs on the
   *  velocity field (element values, element gradient).
   *
   *  @param eqn  the magnetic equation owning the field
   *  @param kind the post-treatment field name (e.g. "energie_magnetique")
   */
  void configurer(const Convection_Diffusion_Magnetic_field& eqn, const Motcle& kind);

  /*! @brief Recomputes the field values from the current equation state,
   *         then forwards the time bookkeeping to Champ_Fonc_base.
   */
  void mettre_a_jour(double tps) override;

private:
  void calculer();

  OBS_PTR(Convection_Diffusion_Magnetic_field) eqn_;
  Motcle kind_;

  // Cached working tables (velocity, velocity gradient, and b gradient at
  // the elements).
  DoubleTab u_elem_;
  DoubleTab gu_elem_;
  DoubleTab gb_elem_;
};

#endif /* Champ_Fonc_MHD_included */
