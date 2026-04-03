add_executable(GenesisApp main.cpp)
target_link_libraries(GenesisApp PRIVATE GenesisEngine)

# Compile shaders and place .spv files next to the executable
compile_shaders(GenesisApp
    "${CMAKE_SOURCE_DIR}/shaders"
)
