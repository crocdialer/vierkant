function(STRINGIFY_SHADERS GLSL_FOLDER GLSL_VALIDATOR)

    set(OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/shaders.hpp")
    set(OUTPUT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/src/shaders.cpp")

    # create output implementation and header
    file(WRITE ${OUTPUT_HEADER}
            "/* Generated file, do not edit! */\n\n"
            "#pragma once\n\n"
            "#include <array>\n\n"
            "namespace vierkant::shaders{\n\n")
    file(WRITE ${OUTPUT_SOURCE}
            "/* Generated file, do not edit! */\n\n"
            "#include \"${PROJECT_NAME}/shaders.hpp\"\n\n"
            "namespace vierkant::shaders{\n")

    # gather all shader files
    file(GLOB GLSL_SOURCE_FILES
            "${GLSL_FOLDER}/*.vert"
            "${GLSL_FOLDER}/*.geom"
            "${GLSL_FOLDER}/*.frag"
            "${GLSL_FOLDER}/*.tesc"
            "${GLSL_FOLDER}/*.tese"
            "${GLSL_FOLDER}/*.comp"
            "${GLSL_FOLDER}/*.rgen"
            "${GLSL_FOLDER}/*.rchit"
            "${GLSL_FOLDER}/*.rmiss")

    foreach (GLSL ${GLSL_SOURCE_FILES})

        get_filename_component(FILE_NAME ${GLSL} NAME)
        string(REGEX REPLACE "[.]" "_" NAME ${FILE_NAME})
        set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${NAME}.spv")
#        message(${SPIRV})
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})

        execute_process(
                COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
                COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
                OUTPUT_QUIET
        )

        # read spirv binary
        file(READ "${SPIRV}" contents HEX)
        string(LENGTH "${contents}" contents_length)
        math(EXPR num_bytes "${contents_length} / 2")

        file(APPEND ${OUTPUT_HEADER} "extern const std::array<unsigned char, ${num_bytes}> ${NAME};\n")
        file(APPEND ${OUTPUT_SOURCE} "\nconst std::array<unsigned char, ${num_bytes}> ${NAME} = {")

        string(REGEX MATCHALL ".." hex_values "${contents}")
        string(REPLACE ";" ", 0x" hex_values "${hex_values}")

        file(APPEND ${OUTPUT_SOURCE} "0x${hex_values}};\n")
    endforeach (GLSL)

    # close namespace
    file(APPEND ${OUTPUT_HEADER} "\n}\n")
    file(APPEND ${OUTPUT_SOURCE} "\n}\n")

    add_custom_target(
            shaders
            DEPENDS ${SPIRV_BINARY_FILES}
    )

endfunction(STRINGIFY_SHADERS)
