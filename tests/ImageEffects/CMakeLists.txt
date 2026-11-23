# The separate image-effect harness rebuilt Artifact production targets in a
# second CMake tree. Image-effect tests now use the repository test graph.
message(FATAL_ERROR
    "The standalone tests/ImageEffects build was retired because it rebuilt Artifact targets in a second CMake tree. Use the shared root build instead: cmake --preset x64-Debug-Agent -DARTIFACT_BUILD_TESTS=ON, then cmake --build --preset x64-Debug-Agent --target ArtifactImageEffectsContractTests ArtifactImageEffectsRuntimeContractTests."
)
