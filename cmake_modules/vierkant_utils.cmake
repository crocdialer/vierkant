macro(SUBDIRLIST result curdir)
    FILE(GLOB children ${curdir}/*)
    SET(dirlist "")
    FOREACH(child ${children})
        IF(IS_DIRECTORY ${child})
            LIST(APPEND dirlist ${child})
        ENDIF()
    ENDFOREACH()
    SET(${result} ${dirlist})
endmacro()

function(STRINGIFY_SHADERS GLSL_FOLDER TARGET_NAME GLSL_VALIDATOR)

    set(TOP_NAMESPACE "vierkant::shaders")

    # remove existing spirv files
    file(GLOB SPIRV_FILES "${PROJECT_BINARY_DIR}/${TARGET_NAME}/*.spv")

    if(SPIRV_FILES)
        file(REMOVE "${SPIRV_FILES}")
    endif()

    set(OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/${TARGET_NAME}.hpp")
    set(OUTPUT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/src/${TARGET_NAME}.cpp")

    # create output implementation and header
    file(WRITE ${OUTPUT_HEADER}
            "/* Generated file, do not edit! */\n\n"
            "#pragma once\n\n"
            "#include <array>\n\n"
            "namespace ${TOP_NAMESPACE}\n{\n\n")
    file(WRITE ${OUTPUT_SOURCE}
            "/* Generated file, do not edit! */\n\n"
            "#include \"${PROJECT_NAME}/${TARGET_NAME}.hpp\"\n\n"
            "namespace ${TOP_NAMESPACE}\n{\n")

    # search subdirs
    subdirlist(SUBDIRS ${GLSL_FOLDER})

#    # add root-dir
#    FILE(GLOB BASE_DIR ${GLSL_FOLDER})
#    set(SUBDIRS ${BASE_DIR} ${SUBDIRS})
#    message("POOP: ${SUBDIRS}")

    foreach (SUBDIR ${SUBDIRS})

        # start namespace for subdir
        get_filename_component(DIR_NAME ${SUBDIR} NAME)

        # gather all shader files
        file(GLOB GLSL_SOURCE_FILES
                "${SUBDIR}/*.vert"
                "${SUBDIR}/*.geom"
                "${SUBDIR}/*.frag"
                "${SUBDIR}/*.tesc"
                "${SUBDIR}/*.tese"
                "${SUBDIR}/*.comp"
                "${SUBDIR}/*.rgen"     # ray generation shader
                "${SUBDIR}/*.rint"     # ray intersection shader
                "${SUBDIR}/*.rahit"    # ray any hit shader
                "${SUBDIR}/*.rchit"    # ray closest hit shader
                "${SUBDIR}/*.rmiss"    # ray miss shader
                "${SUBDIR}/*.rcall"    # ray callable shader
                "${SUBDIR}/*.mesh"
                "${SUBDIR}/*.task")

        if(GLSL_SOURCE_FILES)

            message(STATUS "compiling shaders: ${DIR_NAME}")

            # open namespace
            file(APPEND ${OUTPUT_HEADER} "\nnamespace ${DIR_NAME}\n{\n\n")
            file(APPEND ${OUTPUT_SOURCE} "\nnamespace ${DIR_NAME}\n{\n\n")
        endif()

        foreach (GLSL ${GLSL_SOURCE_FILES})

            get_filename_component(FILE_NAME ${GLSL} NAME)
            string(REGEX REPLACE "[.]" "_" NAME ${FILE_NAME})
            set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${DIR_NAME}_${NAME}.spv")
#                    message(${SPIRV})
            list(APPEND SPIRV_BINARY_FILES ${SPIRV})

            execute_process(
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
                    COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 ${GLSL} -o ${SPIRV}
                    OUTPUT_VARIABLE glslangvalidator_std_out
                    ERROR_VARIABLE glslangvalidator_std_err
                    RESULT_VARIABLE ret
                    #                OUTPUT_QUIET
            )

            if(NOT "${ret}" STREQUAL "0")
                message(WARNING "Failed to compile shader: ${glslangvalidator_std_out}")
            endif()

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

        if(GLSL_SOURCE_FILES)
            # close namespace
            file(APPEND ${OUTPUT_HEADER} "\n}// namespace ${DIR_NAME}\n")
            file(APPEND ${OUTPUT_SOURCE} "\n}// namespace ${DIR_NAME}\n")
        endif()

    endforeach (SUBDIR)

    # close namespace
    file(APPEND ${OUTPUT_HEADER} "\n}// namespace ${TOP_NAMESPACE}\n")
    file(APPEND ${OUTPUT_SOURCE} "\n}// namespace ${TOP_NAMESPACE}\n")

    add_custom_target(
            ${TARGET_NAME}
            DEPENDS ${SPIRV_BINARY_FILES}
    )

endfunction(STRINGIFY_SHADERS)
