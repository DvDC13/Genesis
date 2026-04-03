# CompileShaders.cmake
# Finds glslc from the Vulkan SDK and compiles GLSL shaders to SPIR-V

find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)

if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found! Make sure the Vulkan SDK is installed and VULKAN_SDK is set.")
endif()

function(compile_shaders TARGET SHADER_DIR)
    file(GLOB SHADERS
        "${SHADER_DIR}/*.vert"
        "${SHADER_DIR}/*.frag"
        "${SHADER_DIR}/*.comp"
        "${SHADER_DIR}/*.geom"
        "${SHADER_DIR}/*.tesc"
        "${SHADER_DIR}/*.tese"
    )

    set(SPIRV_OUTPUTS "")

    foreach(SHADER ${SHADERS})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPIRV_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${GLSLC} "${SHADER}" -o "${SPIRV_OUTPUT}"
            DEPENDS ${SHADER}
            COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
            VERBATIM
        )

        list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
    endforeach()

    add_custom_target(${TARGET}_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
    add_dependencies(${TARGET} ${TARGET}_shaders)

    # Copy .spv files next to the executable after build
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV_OUTPUTS} "$<TARGET_FILE_DIR:${TARGET}>"
        COMMENT "Copying compiled shaders to output directory"
    )
endfunction()
