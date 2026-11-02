file(GLOB TOPAS_GPU_SRC CONFIGURE_DEPENDS
    "*.hh"
    "*.cc"
)

add_library(gpu ${TOPAS_GPU_SRC})

if (TOPAS_WITH_CELERITAS)
	celeritas_target_link_libraries(gpu PUBLIC Celeritas::G4)
endif ()

# The physics manager calls into this library. CMP0079 allows linking a target
# defined in another directory (TOPAS requests an older CMake policy level).
cmake_policy(SET CMP0079 NEW)
target_link_libraries(physics gpu)
