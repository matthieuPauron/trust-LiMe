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

# Identify boundary faces by bounding box extent
faces = geom.SubShapeAll(shell, geom.ShapeType["FACE"])
print("nb faces:", len(faces))
face_groups = {}
for f in faces:
    bb = geom.BoundingBox(f)
    xmax = bb[1]
    if abs(xmax - r_outer) < 0.05:
        face_groups.setdefault("wall_outer", []).append(f)
    elif abs(xmax - r_inner) < 0.05:
        face_groups.setdefault("wall_inner", []).append(f)
    else:
        face_groups.setdefault("other", []).append(f)
for name, fs in face_groups.items():
    print(f"  {name}: {len(fs)} faces")

# Mesh
mesh = smesh_gen.Mesh(shell, "vks_shell")
algo1D = mesh.Segment()
algo1D.NumberOfSegments(20)
algo2D = mesh.Triangle()
algo2D.MaxElementArea(0.001)
mesh.AutomaticTetrahedralization()

if not mesh.Compute():
    raise Exception("Mesh computation failed")

# Create groups on each boundary face
groups = []
for name, fs in face_groups.items():
    if name == "other":
        continue
    for f in fs:
        g = mesh.GroupOnGeom(f, name, SMESH.FACE)
        groups.append(g)
        print(f"Created group {name}: size={g.Size()}")

# Export main mesh + append groups
mesh.ExportMED("vks_shell.med", False, 32, True)
for g in groups:
    mesh.ExportMED("vks_shell.med", False, 32, False, meshPart=g)
print("Exported vks_shell.med")
print("Groups:", [g.GetName() for g in groups])

salome.close() if hasattr(salome, 'close') else None
