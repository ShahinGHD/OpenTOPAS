# GPU/CPU offload with Celeritas (`Ts/UseGPU`)

This optional feature hands the transport of electrons, positrons and gammas to
[Celeritas](https://github.com/celeritas-project/celeritas), which runs on an
NVIDIA/AMD GPU or on the CPU. Everything else (protons, ions, neutrons, decay,
scoring, geometry, input files) stays in Geant4/TOPAS as usual.

It is off unless TOPAS is built with `-DTOPAS_WITH_CELERITAS=ON` **and** the
parameter file sets:

```
b:Ts/UseGPU = "True"
```

With the switch off (or a TOPAS built without it) results are identical to
standard TOPAS.

Step-by-step installation: [OpenTOPAS_GPU_install.md](OpenTOPAS_GPU_install.md).
A first test: [examples/WaterPhantomGPU.txt](examples/WaterPhantomGPU.txt).

## Requirements

- Geant4 11.x built with GDML (`-DGEANT4_USE_GDML=ON`, needs `libxerces-c-dev`)
- Celeritas v0.7.0 with `-DCELERITAS_USE_Geant4=ON`, patched with
  `gpu/patches/celeritas_v0.7.0_topas.patch` (fixes wrong voxel numbers in
  replicated geometries and tracks lost when they start on a surface)
- For a GPU: CUDA 12.x and an NVIDIA card of compute capability 6.0 or newer

## Optional parameters

| Parameter | Default | Meaning |
|---|---|---|
| `i:Ts/GPU/MaxNumTracks` | 2048 | Tracks processed together (use ~64 for CPU mode, much higher on a GPU) |
| `i:Ts/GPU/InitializerCapacity` | 128 x MaxNumTracks | Track buffer size |
| `sv:Ts/GPU/IgnoreProcesses` | `1 "CoulombScat"` | Geant4 processes Celeritas skips |
| `s:Ts/GPU/OutputFile` | `celeritas.out.json` | Celeritas diagnostic output |
| `s:Ts/GPU/GeometryOutputFile` | (none) | Write the converted geometry |

## Limitations

TOPAS stops with a clear message if `Ts/UseGPU` is used with:

- magnetic or electric fields
- R or Phi bins (TsCylinder, TsSphere), VoxelMaterials or patient (DICOM/CT) images

Use only X, Y or Z bins with the GPU switch on.

Also:

- Primary beams must start inside the World, not on its boundary.
- **Electron beams of a few MeV and above are not validated.** Celeritas v0.7.0
  produces about 10% fewer and softer bremsstrahlung photons than Geant4, which
  changes the depth dose near the end of the electron range by up to ~30%.

## Validation (Celeritas CPU mode, compared with standard TOPAS)

| Test | Result |
|---|---|
| 6 MeV photons, water depth dose | total +0.09%, every slice within 1.4% of peak |
| 150 MeV protons | total -0.03% |
| 4 threads | total +0.23% |
| Dose, EnergyDeposit, Fluence, EnergyFluence, filters | within ~1% |
| Phase space | photons -0.3%, electrons +0.7% |
| Tb-161, Lu-177, Ac-225 sources in water | within statistics |
| 10 MeV electrons | **not validated** (see above) |
