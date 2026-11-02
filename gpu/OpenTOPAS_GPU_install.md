# Installing OpenTOPAS with GPU support (Celeritas)

This guide installs a version of OpenTOPAS that can hand the transport of
**electrons, positrons and photons** to [Celeritas](https://github.com/celeritas-project/celeritas),
which runs on an NVIDIA GPU (or, for testing, on the CPU). Everything else
(protons, ions, decay, geometry, scorers, your parameter files) works exactly as
in normal TOPAS.

You turn it on per simulation with one line in your parameter file:

```
b:Ts/UseGPU = "True"
```

Without that line, this TOPAS behaves exactly like the official one.

These instructions were tested on **Windows 11 + WSL2 + Ubuntu 22.04** with
OpenTOPAS 4.3.0, Geant4 11.4.2 and Celeritas 0.7.0.

> [!IMPORTANT]
> Read [Limitations](#limitations) before trusting results. In short: no
> magnetic fields, no R/Phi bins, no patient/CT voxels, and **electron beams of a
> few MeV and above are not validated yet.**

---

## The big picture

You will build four things, in this order. Each lives in its own folder under
`~/Applications`:

| Order | What | Why |
|---|---|---|
| 1 | **Geant4** (with GDML turned on) | The physics engine TOPAS is built on. Celeritas needs the GDML option. |
| 2 | **GDCM** | Reads DICOM files. TOPAS needs it. |
| 3 | **Celeritas** (with the TOPAS patch) | The GPU transport engine |
| 4 | **OpenTOPAS** from this repository | TOPAS itself, built with `-DTOPAS_WITH_CELERITAS=ON` |

Each one follows the same three steps: **download the source, configure it
(`cmake`), then compile and install it**.

> [!NOTE]
> Never put `sudo` in front of `make`, `cmake --build` or `cmake --install` in this
> guide. Everything installs in your home folder, so `sudo` is not needed, and it
> leaves files your user cannot change later. Use `sudo` only for `apt-get`.

### CPU mode or GPU mode?

Celeritas can be built for the **CPU** or for an **NVIDIA GPU**.

- **CPU mode** works on any computer. Use it to check that everything is
  installed correctly and gives the same results as normal TOPAS. In CPU mode it
  is **5 to 10 times slower** than normal TOPAS, so it is not for real work.
- **GPU mode** needs an NVIDIA card with compute capability **6.0 or newer**
  (GTX 10xx series or newer, e.g. RTX 3060 = 8.6). Speed-ups only happen here.

Steps that differ between the two are marked **[CPU]** and **[GPU]**.

---

## Step 1: WSL and Ubuntu (Windows only)

If you do not have Ubuntu in WSL yet, follow Steps 1–4 of
[OpenTOPAS_quickStart_WSL.md](../OpenTOPAS_quickStart_WSL.md), but install
Ubuntu 22.04 (`wsl --install Ubuntu-22.04`). On a native Linux computer, skip
this step.

## Step 2: System packages

Open the Ubuntu terminal and run:

```bash
sudo apt-get update
sudo apt-get install -y build-essential git wget cmake ninja-build \
    libexpat1-dev libgl1-mesa-dev libglu1-mesa-dev libxt-dev xorg-dev libharfbuzz-dev \
    qt6-base-dev qt6-base-dev-tools libqt6opengl6-dev libxerces-c-dev nlohmann-json3-dev
```

What the less obvious ones are for:

- `libxerces-c-dev`: lets Geant4 read and write GDML geometry files (Celeritas needs this)
- `nlohmann-json3-dev`: a JSON library Celeritas uses
- `ninja-build`: a fast compiler driver used for Celeritas
- `qt6-base-dev`, `libqt6opengl6-dev`: the graphics window of Geant4/TOPAS

## Step 3 [GPU only]: NVIDIA driver and CUDA

Skip this step for CPU mode.

**On Windows + WSL:**

1. In **Windows**, install the normal NVIDIA driver for your card from nvidia.com
   (Game Ready or Studio). **Do not install any NVIDIA driver inside Ubuntu:** WSL
   uses the Windows one.
2. In the **Ubuntu terminal**, check that the card is visible:

   ```bash
   nvidia-smi
   ```

   You should see your card's name (for example "NVIDIA GeForce RTX 3060").
3. Install the CUDA 12.6 compiler (the WSL version):

   ```bash
   cd ~
   wget https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/cuda-keyring_1.1-1_all.deb
   sudo dpkg -i cuda-keyring_1.1-1_all.deb
   sudo apt-get update
   sudo apt-get install -y cuda-toolkit-12-6
   ```

**On native Ubuntu 22.04:** install the driver with
`sudo ubuntu-drivers install`, reboot, then use the same commands but with
`ubuntu2204` in place of `wsl-ubuntu` in the `wget` link.

The GPU build of Celeritas needs **CMake 3.24 or newer**, but Ubuntu 22.04 comes
with 3.22. Install a newer one (it goes in `/usr/local` and takes priority):

```bash
cd ~
wget https://github.com/Kitware/CMake/releases/download/v3.28.6/cmake-3.28.6-linux-x86_64.sh
sudo sh cmake-3.28.6-linux-x86_64.sh --prefix=/usr/local --skip-license
hash -r
cmake --version
```

The last line should say `cmake version 3.28.6`.

Then, in both cases, make CUDA findable and check it:

```bash
echo 'export PATH=/usr/local/cuda/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
nvcc --version
```

The last line should say `release 12.6`.

Also find your card's **compute capability**, written without the dot. You need
it in Step 6:

```bash
nvidia-smi --query-gpu=name,compute_cap --format=csv
```

For an RTX 3060 this prints `8.6`, so you will use `86`.

## Step 4: Geant4 11.4.2 (with GDML)

This is the same as the official guide, except that it adds
`-DGEANT4_USE_GDML=ON` and uses Qt6.

```bash
mkdir -p ~/Applications/GEANT4 && cd ~/Applications/GEANT4
wget https://gitlab.cern.ch/geant4/geant4/-/archive/v11.4.2/geant4-v11.4.2.tar.gz
tar -zxf geant4-v11.4.2.tar.gz
mkdir geant4-build geant4-install G4DATA
cd geant4-build
cmake ../geant4-v11.4.2 \
    -DCMAKE_INSTALL_PREFIX=~/Applications/GEANT4/geant4-install \
    -DGEANT4_INSTALL_DATA=OFF \
    -DGEANT4_BUILD_MULTITHREADED=ON \
    -DGEANT4_USE_GDML=ON \
    -DGEANT4_USE_QT=ON -DGEANT4_USE_QT_QT6=ON -DCMAKE_PREFIX_PATH=/usr/lib/qt6
make -j$(nproc)
make install
```

`$(nproc)` means "use all CPU cores". The build takes 30–60 minutes.

Then download the Geant4 data files into `~/Applications/GEANT4/G4DATA`, exactly
as in Step 7 of [OpenTOPAS_quickStart_WSL.md](../OpenTOPAS_quickStart_WSL.md) (the
list of `wget` and `tar -zxf` commands).

> [!TIP]
> Already have Geant4 11.4.2 installed for normal TOPAS? Check whether GDML is on:
> `grep GEANT4_USE_GDML ~/Applications/GEANT4/geant4-build/CMakeCache.txt`.
> If it says `OFF`, install `libxerces-c-dev`, then rerun only the `cmake` line
> above (with `-DGEANT4_USE_GDML=ON`), `make -j$(nproc)` and `make install`.
> Your normal TOPAS keeps working.

## Step 5: Download this OpenTOPAS and build GDCM

Download the GPU version of OpenTOPAS (this repository):

```bash
mkdir -p ~/Applications/TOPAS && cd ~/Applications/TOPAS
git clone -b gpu-support https://github.com/ShahinGHD/OpenTOPAS.git OpenTOPAS-gpu-src
```

This puts the source code in `~/Applications/TOPAS/OpenTOPAS-gpu-src`. The
Celeritas patch you need in Step 6 is inside it, at
`OpenTOPAS-gpu-src/gpu/patches/celeritas_v0.7.0_topas.patch`.

Build GDCM from the copy that comes with TOPAS. Skip this if you already
built GDCM for normal TOPAS; just note where its `gdcm-install` folder is.

```bash
mkdir -p ~/Applications/GDCM && cd ~/Applications/GDCM
tar -zxf ~/Applications/TOPAS/OpenTOPAS-gpu-src/gdcm-2.6.8.tar.gz
mkdir gdcm-build gdcm-install
cd gdcm-build
cmake ../gdcm-2.6.8 -DGDCM_BUILD_SHARED_LIBS=ON -DGDCM_BUILD_DOCBOOK_MANPAGES=OFF \
    -DCMAKE_INSTALL_PREFIX=~/Applications/GDCM/gdcm-install
make -j$(nproc) install
```

## Step 6: Celeritas 0.7.0 (patched)

Download Celeritas and switch to version 0.7.0:

```bash
mkdir -p ~/Applications/CELERITAS && cd ~/Applications/CELERITAS
git clone https://github.com/celeritas-project/celeritas.git
cd celeritas
git checkout v0.7.0
```

**Apply the TOPAS patch.** It fixes two Celeritas bugs that give wrong
results in TOPAS: wrong voxel numbers in sliced geometry, and particles lost
when they start exactly on a surface.

```bash
git apply ~/Applications/TOPAS/OpenTOPAS-gpu-src/gpu/patches/celeritas_v0.7.0_topas.patch
git status --short
```

`git status` should list exactly two changed files:

```
 M src/geocel/g4/GeantNavHistoryUpdater.cc
 M src/orange/OrangeTrackView.hh
```

> [!WARNING]
> If `git apply` says `patch does not apply`, you are probably not on v0.7.0
> (check with `git describe --tags`). If it complains on a Windows-downloaded
> copy, the file got Windows line endings: clone the repository inside Ubuntu
> as shown in Step 5.

Now configure Celeritas. Use **one** of the two commands below.

**[CPU]**

```bash
cd ~/Applications/CELERITAS
cmake -S celeritas -B celeritas-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=~/Applications/CELERITAS/celeritas-install \
    -DCMAKE_PREFIX_PATH=~/Applications/GEANT4/geant4-install \
    -DCELERITAS_USE_Geant4=ON \
    -DCELERITAS_USE_CUDA=OFF -DCELERITAS_USE_HIP=OFF -DCELERITAS_USE_OpenMP=ON \
    -DCELERITAS_USE_VecGeom=OFF -DCELERITAS_USE_ROOT=OFF -DCELERITAS_USE_HepMC3=OFF \
    -DCELERITAS_USE_MPI=OFF -DCELERITAS_USE_PNG=OFF -DCELERITAS_USE_covfie=OFF \
    -DCELERITAS_USE_DD4hep=OFF -DCELERITAS_USE_LArSoft=OFF \
    -DCELERITAS_BUILD_APPS=OFF -DCELERITAS_BUILD_TESTS=OFF -DCELERITAS_BUILD_DOCS=OFF
```

**[GPU]** Replace `86` with your card's number from Step 3:

```bash
cd ~/Applications/CELERITAS
cmake -S celeritas -B celeritas-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=~/Applications/CELERITAS/celeritas-install \
    -DCMAKE_PREFIX_PATH=~/Applications/GEANT4/geant4-install \
    -Dlibcudacxx_DIR=/usr/local/cuda/lib64/cmake/libcudacxx \
    -DCELERITAS_USE_Geant4=ON \
    -DCELERITAS_USE_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=86 \
    -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc \
    -DCELERITAS_USE_HIP=OFF -DCELERITAS_USE_OpenMP=OFF \
    -DCELERITAS_USE_VecGeom=OFF -DCELERITAS_USE_ROOT=OFF -DCELERITAS_USE_HepMC3=OFF \
    -DCELERITAS_USE_MPI=OFF -DCELERITAS_USE_PNG=OFF -DCELERITAS_USE_covfie=OFF \
    -DCELERITAS_USE_DD4hep=OFF -DCELERITAS_USE_LArSoft=OFF \
    -DCELERITAS_BUILD_APPS=OFF -DCELERITAS_BUILD_TESTS=OFF -DCELERITAS_BUILD_DOCS=OFF
```

The `cmake` output should end with `Generating done`. Then compile and install
(15–40 minutes):

```bash
cmake --build celeritas-build -j$(nproc)
cmake --install celeritas-build
```

Check that it worked:

```bash
ls ~/Applications/CELERITAS/celeritas-install/lib/cmake/Celeritas
```

This should list some `Celeritas*.cmake` files.

## Step 7: Build OpenTOPAS with GPU support

```bash
cd ~/Applications/TOPAS
cmake -S OpenTOPAS-gpu-src -B OpenTOPAS-build-gpu \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=~/Applications/TOPAS/OpenTOPAS-install-gpu \
    -DCMAKE_PREFIX_PATH="$HOME/Applications/GEANT4/geant4-install;$HOME/Applications/CELERITAS/celeritas-install" \
    -DGDCM_DIR=$HOME/Applications/GDCM/gdcm-install/lib/gdcm-2.6 \
    -DTOPAS_USE_QT=ON -DTOPAS_USE_QT6=ON \
    -DTOPAS_WITH_CELERITAS=ON
```

In the output, look for a line like:

```
-- Found Celeritas v0.7.0 (CUDA: OFF, HIP: OFF, OpenMP: ON)
```

For a GPU build, it should say `CUDA: ON`. Then compile and install:

```bash
cmake --build OpenTOPAS-build-gpu -j$(nproc)
cmake --install OpenTOPAS-build-gpu
```

The program is now at `~/Applications/TOPAS/OpenTOPAS-install-gpu/bin/topas`.

> [!NOTE]
> This uses separate folders (`OpenTOPAS-build-gpu`, `OpenTOPAS-install-gpu`),
> so an existing normal TOPAS installation is not touched.

## Step 8: Make a `topas-gpu` command

This creates a small script so that typing `topas-gpu file.txt` runs this
version. It also tells TOPAS where Geant4, Celeritas and the data files are.

```bash
mkdir -p ~/shellScripts
cat > ~/shellScripts/topas-gpu <<'EOF'
#!/bin/bash
# Runs OpenTOPAS built with Celeritas (Ts/UseGPU)
export QT_QPA_PLATFORM_PLUGIN_PATH=$HOME/Applications/TOPAS/OpenTOPAS-install-gpu/Frameworks
export TOPAS_G4_DATA_DIR=$HOME/Applications/GEANT4/G4DATA
export LD_LIBRARY_PATH=$HOME/Applications/TOPAS/OpenTOPAS-install-gpu/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$HOME/Applications/GEANT4/geant4-install/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$HOME/Applications/CELERITAS/celeritas-install/lib:$LD_LIBRARY_PATH
$HOME/Applications/TOPAS/OpenTOPAS-install-gpu/bin/topas "$@"
EOF
chmod +x ~/shellScripts/topas-gpu
grep -q 'shellScripts' ~/.bashrc || echo 'export PATH=~/shellScripts:$PATH' >> ~/.bashrc
source ~/.bashrc
```

Copy and paste the whole block at once. The `<<'EOF'` ... `EOF` part writes
everything between those markers into the file exactly as shown.

## Step 9: First test

This repository includes a ready-made example:
[examples/WaterPhantomGPU.txt](examples/WaterPhantomGPU.txt). It sends a
6 MeV photon beam into a water box and scores the dose in 40 depth slices.

Run it with the switch **on**:

```bash
mkdir -p ~/gpu-first-test/on && cd ~/gpu-first-test/on
cp ~/Applications/TOPAS/OpenTOPAS-gpu-src/gpu/examples/WaterPhantomGPU.txt .
OMP_NUM_THREADS=1 topas-gpu WaterPhantomGPU.txt
```

Near the start of the output you should see:

```
TOPAS: electron, positron and gamma transport offloaded to Celeritas
```

The dose goes into `DoseZ.csv`. Now run it with the switch **off**, for comparison:

```bash
mkdir -p ~/gpu-first-test/off && cd ~/gpu-first-test/off
sed 's/Ts\/UseGPU = "True"/Ts\/UseGPU = "False"/' ../on/WaterPhantomGPU.txt > WaterPhantomGPU.txt
topas-gpu WaterPhantomGPU.txt
```

The two `DoseZ.csv` files should agree within about 1–2% in every slice (they use
different random numbers, so they are never identical). In our test the total
dose differed by 0.04%.

**[GPU]** While the "on" run is going, open a second Ubuntu terminal and run
`nvidia-smi`. Your `topas` should appear in the process list at the bottom.

## Step 10: Using it in your own simulations

Add to your parameter file:

```
b:Ts/UseGPU = "True"
```

Optional settings:

| Parameter | Default | What it does |
|---|---|---|
| `i:Ts/GPU/MaxNumTracks` | `2048` | How many particles Celeritas moves at once. **CPU:** use `64`. **GPU:** use large values (see below). |
| `i:Ts/GPU/InitializerCapacity` | 128 × MaxNumTracks | Size of the waiting list for new particles. |
| `sv:Ts/GPU/IgnoreProcesses` | `1 "CoulombScat"` | Geant4 processes that Celeritas skips. |
| `s:Ts/GPU/OutputFile` | `celeritas.out.json` | Celeritas's own diagnostic report. |
| `s:Ts/GPU/GeometryOutputFile` | (none) | Writes the geometry as Celeritas sees it. |

**[CPU] tips:** set `i:Ts/GPU/MaxNumTracks = 64` and start TOPAS with
`OMP_NUM_THREADS=1 topas-gpu ...`. Without these it is much slower.

**[GPU] tips:** good starting values for a GPU with 6–12 GB of memory are:

```
i:Ts/GPU/MaxNumTracks        = 65536
i:Ts/GPU/InitializerCapacity = 1048576
```

If you get an out-of-memory error, halve both. If `nvidia-smi` shows the GPU is
mostly idle, double them. Each TOPAS thread (`Ts/NumberOfThreads`) gets its own
set of these buffers on the GPU, so start with 1–4 threads. These GPU values are
a starting point and have not been tuned yet.

## Limitations

TOPAS **stops with a clear error message** if `Ts/UseGPU` is on together with:

- **Magnetic or electric fields**
- **R or Phi bins** (`TsCylinder`/`TsSphere` with `RBins` or `PhiBins`),
  `VoxelMaterials`, or **patient (DICOM/CT) images**. X, Y and Z bins work.

It also needs a modular physics list (`Geant4_Modular`, the TOPAS default, or a
Geant4 reference list).

Things to know:

- **Electron beams are not validated.** For a 10 MeV electron beam, Celeritas
  0.7.0 makes about 10% fewer bremsstrahlung X-rays than Geant4, and they are
  about 18% lower in energy. The depth dose differs by up to ~30% near the end of
  the electron range. This is in Celeritas itself and has been reported to its
  developers. Photon beams, proton beams and the radionuclides tested (Tb-161,
  Lu-177, Ac-225) are not affected.
- **Beams must start inside the World**, not exactly on its edge.
- **CPU mode is slower** than normal TOPAS. It is meant for checking results;
  real speed comes only from a GPU.

### What was tested (CPU mode, compared with normal TOPAS)

| Test | Difference |
|---|---|
| 6 MeV photons, water depth dose | total +0.09%, every slice within 1.4% of the peak |
| 150 MeV protons | total −0.03% |
| 4 threads | total +0.23% |
| Dose, energy deposit, fluence, energy fluence, filters | within ~1% |
| Phase space output | photons −0.3%, electrons +0.7% |
| Tb-161, Lu-177, Ac-225 sources in water | within statistical noise |
| 10 MeV electrons | **not validated** (see above) |

GPU mode uses the same physics code as CPU mode, but it has not yet been tested
on a real GPU.

## Updating

When this repository gets new changes:

```bash
cd ~/Applications/TOPAS/OpenTOPAS-gpu-src
git pull
cmake --build ~/Applications/TOPAS/OpenTOPAS-build-gpu -j$(nproc)
cmake --install ~/Applications/TOPAS/OpenTOPAS-build-gpu
```

You do not need to rebuild Geant4, GDCM or Celeritas unless the patch file
in `patches/` changes. If it does, repeat Step 6, starting from
`git checkout -- .` inside the `celeritas` folder (this removes the old patch),
then rebuild TOPAS.

## Troubleshooting

| Message | What it means / what to do |
|---|---|
| `Ts/UseGPU is True, but this TOPAS was built without GPU support` | You ran the normal `topas`, or built without `-DTOPAS_WITH_CELERITAS=ON`. Use `topas-gpu`. |
| `error while loading shared libraries: libceleritas.so` | Run through `topas-gpu`, not the `topas` program directly (the script sets `LD_LIBRARY_PATH`). |
| `Could not find a package configuration file provided by "Celeritas"` (when building TOPAS) | Celeritas is not installed where `CMAKE_PREFIX_PATH` points. Check Step 6 finished with `cmake --install`. |
| `Could not find ... XercesC` or a GDML error (when building Celeritas) | Geant4 was built without GDML. See the tip in Step 4. |
| `Permission denied` while building | Some files were created with `sudo`. Fix with `sudo chown -R $USER:$USER ~/Applications` and never use `sudo` for building. |
| `Failed to initialize geometry state` warnings | The Celeritas patch is missing. Check `git status` in the `celeritas` folder (Step 6). |
| `Celeritas CUDA error: operation not supported` (or `no kernel image is available`) | Celeritas was compiled for a different card generation. Redo Step 6 with your card's number in `CMAKE_CUDA_ARCHITECTURES`. To run on the CPU anyway, start TOPAS with `CELER_DISABLE_DEVICE=1 topas-gpu ...`. |
| `nvidia-smi: command not found` in WSL | Update the NVIDIA driver in **Windows** and restart WSL (`wsl --shutdown` in a Windows terminal). |
