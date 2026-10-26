# -*- coding: utf-8 -*-
"""
SALOME Python script: VKS true spherical shell with impeller surfaces.
Produces a MED mesh file compatible with TRUST (PolyMAC_MPFA).

Usage (in SALOME):
    salome -t salome_vks_spherical_shell.py
or run interactively in the SALOME Python console.

Output: vks_shell.med  (in the working directory)

Geometry (VKS2, Tassone et al. 2024)
------------------------------------
- Outer sphere radius : 0.206 m
- Inner sphere radius : 0.100 m  (optional; set r_inner = 0 to drop it)
- Impeller radius     : 0.185 m
- Impellers are two flat disks at z = +/- 0.05 m (simplified; replace with
  the real blade geometry imported from STEP for production runs).

TRUST compatibility notes
-------------------------
- Boundary face groups are named to match the .data boundary_conditions keys:
    wall_outer, wall_inner, impeller_top, impeller_bottom
- The volume mesh is tetrahedral; TRUST converts it to polyhedra via
  `Polyedriser dom` (required for PolyMAC_MPFA in 3D).
- MED format: export with MED 3.x (second arg 0) for maximum TRUST compat.
- Node coordinates in meters.
- The shell is the volume between the outer and inner spheres; impeller
  disks are *surfaces* (not volumes) used only to define face groups.
"""

import salome
salome.salome_init()
import GEOM
from salome.geom import geomBuilder
import MESH
from salome.mesh import meshBuilder
import SMESH
from salome.smesh import smeshBuilder

geom = geomBuilder.New()
mesh = meshBuilder.New()

# ---------------------------------------------------------------- geometry --
# VKS2 dimensions (meters).
r_inner = 0.100
r_outer = 0.206
impeller_r = 0.185
impeller_z = 0.050          # axial position of each impeller disk

# Spherical shell = outer sphere minus inner sphere.
outer = geom.MakeSphereR(r_outer)
if r_inner > 0.0:
    inner = geom.MakeSphereR(r_inner)
    shell = geom.MakeCut(outer, inner)
else:
    shell = outer

# Impeller disks: flat circular faces (simplified). In a real VKS mesh these
# would be the actual blade surfaces imported from a STEP file; here we build
# a disk as the top face of a thin cylinder and keep only its face.
def make_disk(r, z):
    cyl = geom.MakeCylinderRH(r, 1e-3)          # thin cylinder
    cyl = geom.MakeTranslation(cyl, 0.0, 0.0, z - 0.5e-3)
    # take the top (or bottom) face as the impeller surface
    faces = geom.SubShapeAll(cyl, geom.FACE)
    # pick the face whose normal points along +Z (top) for z>0, -Z for z<0
    return faces[0] if faces else cyl

disk_top = make_disk(impeller_r, +impeller_z)
disk_bot = make_disk(impeller_r, -impeller_z)

# ---------------------------------------------------------------- meshing --
# Tetrahedral volume mesh on the shell.
mesh_shell = mesh.CreateMesh(shell, "vks_shell")
algo3d = mesh_shell.Tetrahedron(algo=SMESH.MEFISTO)
mesh_shell.Compute()

# Boundary face groups (names must match .data boundary_conditions keys).
grp_outer = mesh_shell.GroupOnGeom(outer, "wall_outer", SMESH.FACE)
if r_inner > 0.0:
    grp_inner = mesh_shell.GroupOnGeom(inner, "wall_inner", SMESH.FACE)
grp_top = mesh_shell.GroupOnGeom(disk_top, "impeller_top", SMESH.FACE)
grp_bot = mesh_shell.GroupOnGeom(disk_bot, "impeller_bottom", SMESH.FACE)

# ---------------------------------------------------------------- export --
mesh_shell.ExportMED("vks_shell.med", 0)   # 0 = MED 3.x (TRUST-compatible)
print("Exported vks_shell.med")
print("Groups: wall_outer, wall_inner, impeller_top, impeller_bottom")

salome.close()