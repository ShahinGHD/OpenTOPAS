# =========================================================
# OpenTOPAS + Geant4 + GDCM + Celeritas (CUDA), Ubuntu 22.04
#
# Build from the repository root:
#   docker build -f gpu/docker/Dockerfile -t opentopas-gpu .
# Run (needs an NVIDIA GPU and driver on the host):
#   docker run --rm --gpus all -v "$PWD":/simulations opentopas-gpu MySimulation.txt
#
# See gpu/docker/README.md
# =========================================================
ARG CUDA_IMAGE_VERSION=12.6.3

FROM nvidia/cuda:${CUDA_IMAGE_VERSION}-devel-ubuntu22.04 AS build

ARG G4_VERSION=11.4.2
ARG CELERITAS_VERSION=v0.7.0
# GPU generations to compile for: 75 = RTX 20xx, 86 = RTX 30xx, 89 = RTX 40xx
ARG CUDA_ARCHITECTURES=86
ARG BUILD_JOBS=4

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git wget ca-certificates \
    libexpat1-dev libgl1-mesa-dev libglu1-mesa-dev libxt-dev xorg-dev libharfbuzz-dev \
    qt6-base-dev qt6-base-dev-tools libqt6opengl6-dev libxerces-c-dev nlohmann-json3-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

ENV APP_HOME=/Applications
WORKDIR $APP_HOME

# ---------------------------------------------------------
# 1. Geant4 (GDML is required by Celeritas)
# ---------------------------------------------------------
RUN mkdir GEANT4 && cd GEANT4 && \
    wget -q https://gitlab.cern.ch/geant4/geant4/-/archive/v${G4_VERSION}/geant4-v${G4_VERSION}.tar.gz && \
    tar -zxf geant4-v${G4_VERSION}.tar.gz && \
    cmake -S geant4-v${G4_VERSION} -B geant4-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$APP_HOME/GEANT4/geant4-install \
        -DGEANT4_INSTALL_DATA=OFF \
        -DGEANT4_BUILD_MULTITHREADED=ON \
        -DGEANT4_USE_GDML=ON \
        -DGEANT4_USE_QT=ON -DGEANT4_USE_QT_QT6=ON -DCMAKE_PREFIX_PATH=/usr/lib/qt6 && \
    cmake --build geant4-build -j${BUILD_JOBS} && \
    cmake --install geant4-build && \
    rm -rf geant4-build geant4-v${G4_VERSION} geant4-v${G4_VERSION}.tar.gz

# ---------------------------------------------------------
# 2. OpenTOPAS source (this repository) and GDCM
# ---------------------------------------------------------
COPY . $APP_HOME/TOPAS/OpenTOPAS

RUN mkdir GDCM && cd GDCM && \
    tar -zxf $APP_HOME/TOPAS/OpenTOPAS/gdcm-2.6.8.tar.gz && \
    cmake -S gdcm-2.6.8 -B gdcm-build \
        -DGDCM_BUILD_SHARED_LIBS=ON \
        -DGDCM_BUILD_DOCBOOK_MANPAGES=OFF \
        -DCMAKE_INSTALL_PREFIX=$APP_HOME/GDCM/gdcm-install && \
    cmake --build gdcm-build -j${BUILD_JOBS} && \
    cmake --install gdcm-build && \
    rm -rf gdcm-build gdcm-2.6.8

# ---------------------------------------------------------
# 3. Celeritas, patched for TOPAS
# ---------------------------------------------------------
# The CUDA build of Celeritas needs CMake 3.24 or newer; Ubuntu 22.04 has 3.22
ARG CMAKE_VERSION=3.28.6
RUN wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh && \
    sh cmake-${CMAKE_VERSION}-linux-x86_64.sh --prefix=/usr/local --skip-license && \
    rm cmake-${CMAKE_VERSION}-linux-x86_64.sh && \
    hash -r && cmake --version

RUN mkdir CELERITAS && cd CELERITAS && \
    git clone --depth 1 --branch ${CELERITAS_VERSION} https://github.com/celeritas-project/celeritas.git && \
    cd celeritas && \
    sed 's/\r$//' $APP_HOME/TOPAS/OpenTOPAS/gpu/patches/celeritas_${CELERITAS_VERSION}_topas.patch > /tmp/celeritas.patch && \
    git apply /tmp/celeritas.patch && \
    cd .. && \
    cmake -S celeritas -B celeritas-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$APP_HOME/CELERITAS/celeritas-install \
        -DCMAKE_PREFIX_PATH=$APP_HOME/GEANT4/geant4-install \
        -Dlibcudacxx_DIR=/usr/local/cuda/lib64/cmake/libcudacxx \
        -DCELERITAS_USE_Geant4=ON \
        -DCELERITAS_USE_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES="${CUDA_ARCHITECTURES}" \
        -DCELERITAS_USE_HIP=OFF -DCELERITAS_USE_OpenMP=OFF \
        -DCELERITAS_USE_VecGeom=OFF -DCELERITAS_USE_ROOT=OFF -DCELERITAS_USE_HepMC3=OFF \
        -DCELERITAS_USE_MPI=OFF -DCELERITAS_USE_PNG=OFF -DCELERITAS_USE_covfie=OFF \
        -DCELERITAS_USE_DD4hep=OFF -DCELERITAS_USE_LArSoft=OFF \
        -DCELERITAS_BUILD_APPS=OFF -DCELERITAS_BUILD_TESTS=OFF -DCELERITAS_BUILD_DOCS=OFF && \
    cmake --build celeritas-build -j${BUILD_JOBS} && \
    cmake --install celeritas-build && \
    rm -rf celeritas-build celeritas

# ---------------------------------------------------------
# 4. OpenTOPAS with Celeritas
# ---------------------------------------------------------
RUN cd TOPAS && \
    cmake -S OpenTOPAS -B OpenTOPAS-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$APP_HOME/TOPAS/OpenTOPAS-install \
        -DCMAKE_PREFIX_PATH="$APP_HOME/GEANT4/geant4-install;$APP_HOME/CELERITAS/celeritas-install" \
        -DGDCM_DIR=$APP_HOME/GDCM/gdcm-install/lib/gdcm-2.6 \
        -DTOPAS_USE_QT=ON -DTOPAS_USE_QT6=ON \
        -DTOPAS_WITH_CELERITAS=ON && \
    cmake --build OpenTOPAS-build -j${BUILD_JOBS} && \
    cmake --install OpenTOPAS-build && \
    rm -rf OpenTOPAS-build

# ---------------------------------------------------------
# 5. Geant4 data (optional; skip with --build-arg INCLUDE_G4DATA=0
#    and mount your own at /Applications/G4Data instead)
# ---------------------------------------------------------
ARG INCLUDE_G4DATA=1
RUN mkdir -p $APP_HOME/G4Data && \
    if [ "$INCLUDE_G4DATA" = "1" ]; then \
        cd $APP_HOME/G4Data && \
        sed 's/\r$//' $APP_HOME/TOPAS/OpenTOPAS/.github/workflows/g4data_urls.txt | while read -r url; do \
            [ -n "$url" ] || continue; \
            wget -q "$url" && tar -zxf "$(basename "$url")" && rm "$(basename "$url")" || exit 1; \
        done; \
    fi

# =========================================================
# Runtime image: only what is needed to run TOPAS
# =========================================================
FROM nvidia/cuda:${CUDA_IMAGE_VERSION}-runtime-ubuntu22.04

ARG G4_VERSION=11.4.2
LABEL description="OpenTOPAS with Celeritas GPU offload (Ts/UseGPU), Geant4 ${G4_VERSION}"

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libexpat1 libxerces-c3.2 libgl1 libglu1-mesa libxt6 libharfbuzz0b \
    libqt6core6 libqt6gui6 libqt6widgets6 libqt6opengl6 libqt6openglwidgets6 libqt6printsupport6 \
    qt6-qpa-plugins xvfb && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

ENV APP_HOME=/Applications
COPY --from=build $APP_HOME/GEANT4/geant4-install $APP_HOME/GEANT4/geant4-install
COPY --from=build $APP_HOME/GDCM/gdcm-install $APP_HOME/GDCM/gdcm-install
COPY --from=build $APP_HOME/CELERITAS/celeritas-install $APP_HOME/CELERITAS/celeritas-install
COPY --from=build $APP_HOME/TOPAS/OpenTOPAS-install $APP_HOME/TOPAS/OpenTOPAS-install
COPY --from=build $APP_HOME/G4Data $APP_HOME/G4Data

ENV TOPAS_G4_DATA_DIR=$APP_HOME/G4Data \
    QT_QPA_PLATFORM_PLUGIN_PATH=$APP_HOME/TOPAS/OpenTOPAS-install/Frameworks \
    LD_LIBRARY_PATH=$APP_HOME/TOPAS/OpenTOPAS-install/lib:$APP_HOME/GEANT4/geant4-install/lib:$APP_HOME/GDCM/gdcm-install/lib:$APP_HOME/CELERITAS/celeritas-install/lib \
    PATH=$APP_HOME/TOPAS/OpenTOPAS-install/bin:$PATH

# Same entrypoint as the standard OpenTOPAS image: a virtual display unless
# TOPAS_USE_HOST_DISPLAY is set
RUN cat <<'EOF' > /usr/local/bin/topas-entrypoint.sh && chmod +x /usr/local/bin/topas-entrypoint.sh
#!/bin/bash
set -euo pipefail

XVFB_DISPLAY="${TOPAS_XVFB_DISPLAY:-:99}"
XVFB_SCREEN="${TOPAS_XVFB_SCREEN:-1280x800x24}"
USE_HOST_DISPLAY="${TOPAS_USE_HOST_DISPLAY:-}"

cleanup() {
  if [[ -n "${XVFB_PID:-}" ]]; then
    kill "${XVFB_PID}" >/dev/null 2>&1 || true
  fi
}

if [[ -n "$USE_HOST_DISPLAY" && -n "${DISPLAY:-}" ]]; then
  echo "Using host DISPLAY ${DISPLAY}"
else
  Xvfb "${XVFB_DISPLAY}" -screen 0 "${XVFB_SCREEN}" >/dev/null 2>&1 &
  XVFB_PID=$!
  trap cleanup EXIT
  export DISPLAY="${XVFB_DISPLAY}"
fi

topas "$@"
EOF

COPY --from=build $APP_HOME/TOPAS/OpenTOPAS/gpu/examples /examples
RUN mkdir -p /simulations
WORKDIR /simulations

ENTRYPOINT ["/usr/local/bin/topas-entrypoint.sh"]
