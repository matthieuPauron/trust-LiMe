import salome, os
salome.salome_init()
import SMESH
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder

geom = geomBuilder.New()
smesh_gen = smeshBuilder.New()

r_cyl = 0.206
h_cyl = 0.524
mesh_size = float(os.environ.get("MESH_SIZE", "0.04"))
out_name = os.environ.get("OUT_NAME", "vks_cyl")

# Geometry: cylinder (no partition)
cyl = geom.MakeCylinderRH(r_cyl, h_cyl)
cyl = geom.MakeTranslation(cyl, 0, 0, -h_cyl/2.0)

# Faces
faces = geom.SubShapeAll(cyl, geom.ShapeType["FACE"])
print("nb faces:", len(faces))
for i, f in enumerate(faces):
    bb = geom.BoundingBox(f)
    print(f"  face {i}: x=[{bb[0]:.3f},{bb[1]:.3f}] y=[{bb[2]:.3f},{bb[3]:.3f}] z=[{bb[4]:.3f},{bb[5]:.3f}]")

# Classify
groups_faces = {"wall_cyl": [], "wall_top": [], "wall_bottom": []}
for f in faces:
    bb = geom.BoundingBox(f)
    zmin, zmax = bb[4], bb[5]
    xmin, xmax = bb[0], bb[1]
    if abs(zmin - h_cyl/2.0) < 1e-6 and abs(zmax - h_cyl/2.0) < 1e-6:
        groups_faces["wall_top"].append(f)
    elif abs(zmin + h_cyl/2.0) < 1e-6 and abs(zmax + h_cyl/2.0) < 1e-6:
        groups_faces["wall_bottom"].append(f)
    else:
        groups_faces["wall_cyl"].append(f)

# Mesh
mesh = smesh_gen.Mesh(cyl, "vks_cyl")
algo1D = mesh.Segment()
algo1D.NumberOfSegments(max(10, int(1.0/mesh_size)))
algo2D = mesh.Triangle()
algo2D.MaxElementArea(mesh_size*mesh_size)
algo3D = mesh.Tetrahedron()
algo3D.MaxElementVolume(mesh_size**3 / 6.0)

if not mesh.Compute():
    raise Exception("Mesh computation failed")

groups = []
for name, fs in groups_faces.items():
    if not fs:
        continue
    comp = geom.MakeCompound(fs) if len(fs) > 1 else fs[0]
    g = mesh.GroupOnGeom(comp, name, SMESH.FACE)
    groups.append(g)
    print(f"Created group {name}: size={g.Size()}")

out_file = out_name + ".med"
mesh.ExportMED(out_file, False, 32, True)
for g in groups:
    mesh.ExportMED(out_file, False, 32, False, meshPart=g)
print(f"Exported {out_file}")

salome.close() if hasattr(salome, 'close') else None
