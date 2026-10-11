/****************************************************************************
 * trust-LiMe — unit tests for the magnetic equation and the problem shell.
 *
 * These tests need no mesh, no time scheme and no solver: they pin the
 * *business-logic contracts* of the module that would otherwise only be
 * exercised at the end of a full TRUST run.
 *
 * See tests/UnitTests/README.txt for how to build and run.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Convection_Diffusion_Magnetic_field.h>
#include <Pb_MHD.h>
#include <Probleme_base.h>
#include <Cond_lim_base.h>

// ---------------------------------------------------------------------------
// The equation
// ---------------------------------------------------------------------------

/*! The data-file keyword must stay "Convection_Diffusion_Magnetic_field":
 *  renaming it silently breaks every existing .data file and every notebook.
 */
TEST(MagneticField, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_STREQ(eqn.le_type(), "Convection_Diffusion_Magnetic_field");
}

/*! Boundary-condition compatibility contract.
 *
 *  Cond_lim_base::compatible_avec_eqn() matches the equation's
 *  domaine_application() against each CL's supported domains. The module
 *  deliberately reports "Thermique" so that the kernel's scalar (thermal) CLs
 *  are accepted and no magnetic CL class has to be written. If this ever
 *  changes, every boundary_condition block in the datasets silently stops
 *  being compatible, so the value is pinned here.
 */
TEST(MagneticField, DeclaresThermalDomainSoKernelScalarBCsStayCompatible)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_EQ(eqn.domaine_application(), Motcle("Thermique"));
}

/*! The number of transported components is unresolved (-1) until discretiser()
 *  runs; discretiser() then defaults it to the space dimension. This test only
 *  documents that no value leaks in at construction time.
 */
TEST(MagneticField, ComponentCountIsUnresolvedBeforeDiscretisation)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_EQ(eqn.nb_composantes(), -1);
}

// ---------------------------------------------------------------------------
// The problem shell
// ---------------------------------------------------------------------------

TEST(PbMHD, IsAProblemeBase)
{
  Pb_MHD pb;
  EXPECT_NE(dynamic_cast<Probleme_base *>(&pb), nullptr);
}

/*! Decision D1: this phase is magnetic-only, the problem holds exactly one
 *  equation. Adding a second equation without revisiting Pb_MHD::equation()
 *  would silently expose a wrong accessor.
 */
TEST(PbMHD, HoldsExactlyOneEquation)
{
  Pb_MHD pb;
  EXPECT_EQ(pb.nombre_d_equations(), 1);
}

TEST(PbMHD, EquationZeroIsTheMagneticField)
{
  Pb_MHD pb;
  const Equation_base &eqn = pb.equation(0);
  EXPECT_STREQ(eqn.le_type(), "Convection_Diffusion_Magnetic_field");
}

/*! An out-of-range index must abort loudly rather than return an arbitrary
 *  equation (the kernel's Pb_Conduction only uses assert(), which is compiled
 *  out in -O3; Pb_MHD deliberately exits instead).
 */
TEST(PbMHDDeath, RejectsAnOutOfRangeEquationIndex)
{
  Pb_MHD pb;
  EXPECT_DEATH(static_cast<void>(pb.equation(1)), "");
  EXPECT_DEATH(static_cast<void>(pb.equation(-1)), "");
}