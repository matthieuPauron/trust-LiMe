import salome, os
salome.salome_init()
import SMESH
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder

geom = geomBuilder.New()
smesh_gen = smeshBuilder.New()

r_inner = 0.100
r_outer = 0.206
mesh_size = float(os.environ.get("MESH_SIZE", "0.02"))
out_name = os.environ.get("OUT_NAME", "vks_shell")

# Geometry: spherical shell (no partition)
outer = geom.MakeSphereR(r_outer)
inner = geom.MakeSphereR(r_inner)
shell = geom.MakeCut(outer, inner)

# Identify boundary faces by bounding box
faces = geom.SubShapeAll(shell, geom.ShapeType["FACE"])
print("nb faces:", len(faces))
groups_faces = {"wall_outer": [], "wall_inner": []}
for f in faces:
    bb = geom.BoundingBox(f)
    xmax = bb[1]
    if abs(xmax - r_outer) < 0.05:
        groups_faces["wall_outer"].append(f)
    elif abs(xmax - r_inner) < 0.05:
        groups_faces["wall_inner"].append(f)
    else:
        groups_faces.setdefault("other", []).append(f)
for name, fs in groups_faces.items():
    print(f"  {name}: {len(fs)} faces")

# Mesh
mesh = smesh_gen.Mesh(shell, "vks_shell")
algo1D = mesh.Segment()
algo1D.NumberOfSegments(max(10, int(1.0/mesh_size)))
algo2D = mesh.Triangle()
algo2D.MaxElementArea(mesh_size*mesh_size)
algo3D = mesh.Tetrahedron()
algo3D.MaxElementVolume(mesh_size**3 / 6.0)

if not mesh.Compute():
    raise Exception("Mesh computation failed")

# Create groups via compound
groups = []
for name, fs in groups_faces.items():
    if name == "other" or not fs:
        continue
    comp = geom.MakeCompound(fs) if len(fs) > 1 else fs[0]
    g = mesh.GroupOnGeom(comp, name, SMESH.FACE)
    groups.append(g)
    print(f"Created group {name}: size={g.Size()}")

# Export
out_file = out_name + ".med"
mesh.ExportMED(out_file, False, 32, True)
for g in groups:
    mesh.ExportMED(out_file, False, 32, False, meshPart=g)
print(f"Exported {out_file}")

salome.close() if hasattr(salome, 'close') else None
