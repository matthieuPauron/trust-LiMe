# MED mesh compatibility with TRUST (trust-LiMe)

This note documents how to produce a MED mesh that TRUST (and therefore
trust-LiMe) can read with `Read_MED`, and how to wire it into a `.data` file.

## 1. Read the mesh in the data file

```
# BEGIN MESH #
Read_MED {
    domain dom
    file vks_shell.med
}
Polyedriser dom          # required in 3D for PolyMAC_MPFA
# END MESH #
```

The `# BEGIN MESH #` / `# END MESH #` markers are required by `make_PAR.data`
so it can strip the mesh block and inject `Scatter` for the parallel run.

## 2. Boundary face groups

TRUST matches boundary condition keys to **face group names** in the MED file.
Every group named in `boundary_conditions { ... }` must exist as a face group
in the MED mesh, e.g.:

```
boundary_conditions
{
    wall_outer   paroi_isolante_magnetique
    impeller_top  paroi_defilante Champ_Front_Fonc_xyz 3 ...
}
```

requires face groups `wall_outer`, `impeller_top`, etc. in `vks_shell.med`.
Group names are case-sensitive.

## 3. Volume elements

- TRUST PolyMAC_MPFA operates on **polyhedra**. Tetrahedral (or hexahedral)
  meshes must be converted with `Polyedriser dom` after `Read_MED`.
- Mixed element types in a single mesh are not supported; use one element
  family (tetrahedra recommended for complex geometries).

## 4. MED format version

- TRUST's `Lire_MED` supports MED 3.x and 4.x. Export with MED 3.x for
  maximum compatibility (`mesh.ExportMED("file.med", 0)` in SALOME, where
  the second argument `0` selects MED 3.x).
- Node coordinates in SI units (meters).

## 5. Parallel runs

- Mesh partitioning is done by `make_PAR.data <file.data> <N>` (metis).
- The `# BEGIN PARTITION` / `# BEGIN SCATTER` blocks must be present.
- `Larg_joint 2` is required for PolyMAC_MPFA.

## 6. SALOME script

See `geometry/salome_vks_spherical_shell.py` for a starting-point SALOME
script that builds a spherical shell with impeller surfaces and exports a
TRUST-compatible MED file. Replace the simplified disk impellers with the
real blade geometry (imported STEP) for production runs.

## 7. Documentation

- SALOME documentation: https://docs.salome-platform.org/latest/main/index.html
  - GEOM (geometry) module: https://docs.salome-platform.org/latest/main/geometry/GEOM/index.html
  - SMESH (meshing) module: https://docs.salome-platform.org/latest/main/mesh/SMESH/index.html
- MED format (the exchange format used by TRUST's `Read_MED`):
  - MEDCoupling / MED file documentation: https://docs.salome-platform.org/latest/main/medcoupling/index.html
  - MED library: https://www.hdfgroup.org/solutions/high-performance-computing/ (HDF5-based format)
- TRUST data file keyword reference: https://cea-trust-platform.readthedocs.io/
