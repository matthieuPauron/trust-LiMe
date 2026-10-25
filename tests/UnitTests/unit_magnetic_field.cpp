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
#include <Pb_Induction.h>
#include <Pb_MHD.h>
#include <Pb_MHD_Turbulent.h>
#include <Pb_MHD_Thermal.h>
#include <Paroi_conductrice_magnetique.h>
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

TEST(PbInduction, IsAProblemeBase)
{
  Pb_Induction pb;
  EXPECT_NE(dynamic_cast<Probleme_base *>(&pb), nullptr);
}

/*! The standalone induction problem holds exactly one equation: the magnetic
 *  induction equation. There is no momentum equation by design (the transport
 *  velocity is prescribed), so the class derives from Probleme_base and not
 *  from Pb_Fluide_base.
 */
TEST(PbInduction, HoldsExactlyOneEquation)
{
  Pb_Induction pb;
  EXPECT_EQ(pb.nombre_d_equations(), 1);
}

TEST(PbInduction, EquationZeroIsTheMagneticField)
{
  Pb_Induction pb;
  const Equation_base &eqn = pb.equation(0);
  EXPECT_STREQ(eqn.le_type(), "Convection_Diffusion_Magnetic_field");
}

/*! An out-of-range index must abort loudly rather than return an arbitrary
 *  equation (the kernel's Pb_Conduction only uses assert(), which is compiled
 *  out in -O3; Pb_Induction deliberately exits instead).
 */
TEST(PbInductionDeath, RejectsAnOutOfRangeEquationIndex)
{
  Pb_Induction pb;
  EXPECT_DEATH(static_cast<void>(pb.equation(1)), "");
  EXPECT_DEATH(static_cast<void>(pb.equation(-1)), "");
}

/*! The coupled problem lays the equations out the way the kernel expects:
 *  the momentum equation first (index 0, inherited from Pb_Hydraulique) and
 *  the magnetic induction equation second. The order is structural: the
 *  kernel reads data-file equation names against equation(i) in index order
 *  and discretises the medium through equation(0).
 */
TEST(PbMHD, IsAHydraulicProblem)
{
  Pb_MHD pb;
  EXPECT_NE(dynamic_cast<Pb_Hydraulique *>(&pb), nullptr);
}

TEST(PbMHD, HoldsMomentumThenMagneticField)
{
  Pb_MHD pb;
  EXPECT_EQ(pb.nombre_d_equations(), 2);
  EXPECT_STREQ(pb.equation(0).le_type(), "Navier_Stokes_standard");
  EXPECT_STREQ(pb.equation(1).le_type(), "Convection_Diffusion_Magnetic_field");
}

TEST(PbMHDDeath, RejectsAnOutOfRangeEquationIndex)
{
  Pb_MHD pb;
  EXPECT_DEATH(static_cast<void>(pb.equation(2)), "");
  EXPECT_DEATH(static_cast<void>(pb.equation(-1)), "");
}

// ---------------------------------------------------------------------------
// Pb_MHD_Turbulent
// ---------------------------------------------------------------------------

TEST(PbMHDTurbulent, IsAHydraulicProblem)
{
  Pb_MHD_Turbulent pb;
  EXPECT_NE(dynamic_cast<Pb_Hydraulique_Turbulent *>(&pb), nullptr);
}

TEST(PbMHDTurbulent, HoldsMomentumThenMagneticField)
{
  Pb_MHD_Turbulent pb;
  EXPECT_EQ(pb.nombre_d_equations(), 2);
  EXPECT_STREQ(pb.equation(0).le_type(), "Navier_Stokes_Turbulent");
  EXPECT_STREQ(pb.equation(1).le_type(), "Convection_Diffusion_Magnetic_field");
}

TEST(PbMHDTurbulentDeath, RejectsAnOutOfRangeEquationIndex)
{
  Pb_MHD_Turbulent pb;
  EXPECT_DEATH(static_cast<void>(pb.equation(2)), "");
  EXPECT_DEATH(static_cast<void>(pb.equation(-1)), "");
}

// ---------------------------------------------------------------------------
// Pb_MHD_Thermal
// ---------------------------------------------------------------------------

TEST(PbMHDThermal, HoldsMomentumTemperatureThenMagneticField)
{
  Pb_MHD_Thermal pb;
  EXPECT_EQ(pb.nombre_d_equations(), 3);
  EXPECT_STREQ(pb.equation(0).le_type(), "Navier_Stokes_standard");
  EXPECT_STREQ(pb.equation(1).le_type(), "Convection_Diffusion_Temperature");
  EXPECT_STREQ(pb.equation(2).le_type(), "Convection_Diffusion_Magnetic_field");
}

TEST(PbMHDThermalDeath, RejectsAnOutOfRangeEquationIndex)
{
  Pb_MHD_Thermal pb;
  EXPECT_DEATH(static_cast<void>(pb.equation(3)), "");
  EXPECT_DEATH(static_cast<void>(pb.equation(-1)), "");
}

// ---------------------------------------------------------------------------
// Conducting-wall boundary condition
// ---------------------------------------------------------------------------

TEST(ParoiConductriceMagnetique, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Paroi_conductrice_magnetique cl;
  EXPECT_STREQ(cl.le_type(), "paroi_conductrice_magnetique");
}

// ---------------------------------------------------------------------------
// Convection_Diffusion_Magnetic_field defaults
// ---------------------------------------------------------------------------

TEST(MagneticField, CorrectionDivergenceIsOffByDefault)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_FALSE(eqn.correction_divergence());
}

TEST(MagneticField, VitesseHydrauliqueIsOffByDefault)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_FALSE(eqn.vitesse_hydraulique());
}

TEST(MagneticField, CouplageFortIsOffByDefault)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_FALSE(eqn.couplage_fort());
}

TEST(MagneticField, Mu0DefaultsToOne)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_DOUBLE_EQ(eqn.mu0(), 1.0);
}

TEST(MagneticField, RapportChargeMasseDefaultsToElectronValue)
{
  Convection_Diffusion_Magnetic_field eqn;
  EXPECT_DOUBLE_EQ(eqn.rapport_charge_masse(), 1.75882001076e11);
}