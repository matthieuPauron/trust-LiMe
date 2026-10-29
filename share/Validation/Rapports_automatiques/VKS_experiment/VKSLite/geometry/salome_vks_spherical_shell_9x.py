import salome
salome.salome_init()
import SMESH
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder

geom = geomBuilder.New()
smesh_gen = smeshBuilder.New()

r_inner = 0.100
r_outer = 0.206

# Geometry: spherical shell
outer = geom.MakeSphereR(r_outer)
inner = geom.MakeSphereR(r_inner)
shell = geom.MakeCut(outer, inner)

# Mesh
mesh = smesh_gen.Mesh(shell, "vks_shell")
algo1D = mesh.Segment()
algo1D.NumberOfSegments(20)
algo2D = mesh.Triangle()
algo2D.MaxElementArea(0.001)
mesh.AutomaticTetrahedralization()

if not mesh.Compute():
    raise Exception("Mesh computation failed")

# Groups on boundary surfaces (outer and inner spheres are boundaries of the shell)
grp_outer = mesh.GroupOnGeom(outer, "wall_outer", SMESH.FACE)
grp_inner = mesh.GroupOnGeom(inner, "wall_inner", SMESH.FACE)

# Export main mesh (groups may not be included; try with auto_groups=False first)
mesh.ExportMED("vks_shell.med", False, 32, True)
print("Exported vks_shell.med")
print("Groups:", grp_outer.GetName(), grp_inner.GetName())

salome.close() if hasattr(salome, 'close') else None
