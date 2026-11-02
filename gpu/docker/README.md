# OpenTOPAS GPU Docker image

A Docker image with everything already built: Geant4 11.4.2, GDCM,
Celeritas 0.7.0 (CUDA, with the TOPAS patch), OpenTOPAS with `Ts/UseGPU`, and
the Geant4 data files. Nothing else needs to be installed except Docker and
the NVIDIA driver.

By default it is compiled for **RTX 30xx** cards (`CUDA_ARCHITECTURES=86`). For
another card, see [Building for other GPUs](#building-for-other-gpus).

## 1. Prepare the computer (once)

On a **Windows** laptop with an NVIDIA card:

1. Install the latest NVIDIA driver for Windows from nvidia.com.
2. Install WSL2 and Ubuntu (see [OpenTOPAS_quickStart_WSL.md](../../OpenTOPAS_quickStart_WSL.md), Steps 1–4).
3. Install [Docker Desktop](https://www.docker.com/products/docker-desktop/).
   During setup keep **Use WSL 2** selected. Afterwards, in Docker Desktop go to
   **Settings → Resources → WSL integration** and turn on your Ubuntu.
4. Open the Ubuntu terminal and check that Docker can see the GPU:

   ```bash
   docker run --rm --gpus all nvidia/cuda:12.6.3-base-ubuntu22.04 nvidia-smi
   ```

   You should see a table with your card's name (e.g. "GeForce RTX 3060").

On **native Linux**: install the NVIDIA driver, Docker Engine and the
[NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html),
then run the same check.

## 2. Get the image

Use **one** of these.

**A. Build it on this computer** (about 1–2 hours, needs ~30 GB free disk):

```bash
cd ~
git clone -b gpu-support https://github.com/ShahinGHD/OpenTOPAS.git
cd OpenTOPAS
docker build -f gpu/docker/Dockerfile -t opentopas-gpu .
```

**B. Copy it from a computer that already built it.** On that computer:

```bash
docker save opentopas-gpu | gzip > opentopas-gpu.tar.gz
```

Copy `opentopas-gpu.tar.gz` (several GB) with a USB drive, then on the new computer:

```bash
docker load < opentopas-gpu.tar.gz
```

Check that it is there with `docker images`.

## 3. Run a simulation

Go to the folder with your parameter file and run:

```bash
docker run --rm --gpus all --user "$(id -u):$(id -g)" -v "$PWD":/simulations opentopas-gpu MySimulation.txt
```

- `--gpus all` gives the container the GPU. Without it, Celeritas cannot use the card.
- `--user "$(id -u):$(id -g)"` makes the result files belong to you instead of root.
- `-v "$PWD":/simulations` shares your current folder with the container, so
  TOPAS reads your parameter file from it and writes the results back into it.
- Everything after the image name is passed to `topas`.

Your parameter file must contain `b:Ts/UseGPU = "True"` to use the GPU (see
[gpu/README.md](../README.md)). Without it, it runs as normal TOPAS.

## 4. First GPU test

The example [gpu/examples/WaterPhantomGPU.txt](../examples/WaterPhantomGPU.txt)
is inside the image at `/examples`. Copy it out, then change it to GPU-sized
batches (the file is set up for CPU mode):

```bash
mkdir -p ~/gpu-test/on ~/gpu-test/off
cd ~/gpu-test/on
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/simulations --entrypoint cp opentopas-gpu \
    /examples/WaterPhantomGPU.txt /simulations/
sed -i 's/MaxNumTracks = 64/MaxNumTracks = 65536/' WaterPhantomGPU.txt
echo 'i:Ts/GPU/InitializerCapacity = 1048576' >> WaterPhantomGPU.txt
time docker run --rm --gpus all --user "$(id -u):$(id -g)" -v "$PWD":/simulations opentopas-gpu WaterPhantomGPU.txt
```

While it runs, `nvidia-smi` in a second terminal should show the GPU busy.

Then the same with the switch off, for comparison:

```bash
cd ~/gpu-test/off
sed 's/Ts\/UseGPU = "True"/Ts\/UseGPU = "False"/' ../on/WaterPhantomGPU.txt > WaterPhantomGPU.txt
time docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/simulations opentopas-gpu WaterPhantomGPU.txt
```

Compare the two `DoseZ.csv` files (they should agree within 1–2% per slice) and
the two times.

## If something goes wrong

| Message | What to do |
|---|---|
| `could not select device driver "" with capabilities: [[gpu]]` | Docker cannot see the GPU. Redo the check in step 1 (driver, WSL integration, or NVIDIA Container Toolkit on Linux). |
| `Celeritas CUDA error: operation not supported` or `no kernel image is available` | The image was built for another card generation. Rebuild with the right `CUDA_ARCHITECTURES` (below). |
| You want to run without a GPU | Add `-e CELER_DISABLE_DEVICE=1` (and drop `--gpus all`). Celeritas then runs on the CPU, which is slow but gives the same results. |

## Building for other GPUs

Add `--build-arg CUDA_ARCHITECTURES=<number>` to `docker build`:

| Card | Number |
|---|---|
| RTX 20xx, GTX 16xx | `75` |
| RTX 30xx, A-series (A100 = `80`) | `86` |
| RTX 40xx | `89` |
| Several at once | `"75;86;89"` (slower to build) |

Other build options:

- `--build-arg INCLUDE_G4DATA=0`: leave the Geant4 data out (the image is
  then several GB smaller). Mount your own with
  `-v /path/to/G4DATA:/Applications/G4Data:ro` when running.
- `--build-arg BUILD_JOBS=8`: compile with more CPU cores (needs more memory;
  4 is safe with 16 GB).
