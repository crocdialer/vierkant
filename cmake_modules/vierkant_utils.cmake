macro(SUBDIRLIST result curdir)
    FILE(GLOB children ${curdir}/*)
    SET(dirlist "")
    FOREACH (child ${children})
        IF (IS_DIRECTORY ${child})
            LIST(APPEND dirlist ${child})
        ENDIF ()
    ENDFOREACH ()
    SET(${result} ${dirlist})
endmacro()

function(GET_SHADERS_RECURSIVE RESULT GLSL_FOLDER)

    # search subdirs
    subdirlist(SUBDIRS ${GLSL_FOLDER})

    foreach (SUBDIR ${SUBDIRS})

        # gather main shader files from subdir
        get_shader_sources(GLSL_FOLDER_FILES ${SUBDIR})

        file(GLOB GLSL_INLINE_FILES "${SUBDIR}/*.glsl")

        list(APPEND ALL_SOURCES ${GLSL_FOLDER_FILES} ${GLSL_INLINE_FILES})

    endforeach (SUBDIR)

    set(${RESULT} ${ALL_SOURCES} PARENT_SCOPE)
endfunction(GET_SHADERS_RECURSIVE)

function(GET_SHADER_SOURCES RESULT GLSL_FOLDER)

    # gather all shader files
    file(GLOB GLSL_SOURCE_FILES
            "${GLSL_FOLDER}/*.vert"
            "${GLSL_FOLDER}/*.geom"
            "${GLSL_FOLDER}/*.frag"
            "${GLSL_FOLDER}/*.tesc"
            "${GLSL_FOLDER}/*.tese"
            "${GLSL_FOLDER}/*.comp"
            "${GLSL_FOLDER}/*.rgen"     # ray generation shader
            "${GLSL_FOLDER}/*.rint"     # ray intersection shader
            "${GLSL_FOLDER}/*.rahit"    # ray any hit shader
            "${GLSL_FOLDER}/*.rchit"    # ray closest hit shader
            "${GLSL_FOLDER}/*.rmiss"    # ray miss shader
            "${GLSL_FOLDER}/*.rcall"    # ray callable shader
            "${GLSL_FOLDER}/*.mesh"
            "${GLSL_FOLDER}/*.task")

    set(${RESULT} ${GLSL_SOURCE_FILES} PARENT_SCOPE)
endfunction(GET_SHADER_SOURCES)

function(STRINGIFY_SHADERS GLSL_FOLDER TARGET_NAME SHADER_COMPILER SPIRV_OUT_DIR SOURCE_OUT_DIR)

    # NOTE: seeing some issues with spirv-reflect/local-sizes between 1.2<->1.3
    set(SPIRV_TARGET_ENV vulkan1.2)
    set(TOP_NAMESPACE "vierkant::shaders")

    # remove existing spirv files
    file(GLOB SPIRV_FILES "${SPIRV_OUT_DIR}/shaders/*.spv")

    if (SPIRV_FILES)
        file(REMOVE "${SPIRV_FILES}")
    endif ()

    set(OUTPUT_HEADER "${SOURCE_OUT_DIR}/include/${TARGET_NAME}/shaders.hpp")
    set(OUTPUT_SOURCE "${SOURCE_OUT_DIR}/src/shaders.cpp")

#    message("OUTPUT_HEADER: ${OUTPUT_HEADER}")
#    message("OUTPUT_SOURCE: ${OUTPUT_SOURCE}")

    # create output implementation and header
    file(WRITE ${OUTPUT_HEADER}
            "/* Generated file, do not edit! */\n\n"
            "#pragma once\n\n"
            "#include <array>\n\n"
            "namespace ${TOP_NAMESPACE}\n{\n\n")
    file(WRITE ${OUTPUT_SOURCE}
            "/* Generated file, do not edit! */\n\n"
            "#include \"${TARGET_NAME}/shaders.hpp\"\n\n"
            "namespace ${TOP_NAMESPACE}\n{\n")

    # search subdirs
    subdirlist(SUBDIRS ${GLSL_FOLDER})

    foreach (SUBDIR ${SUBDIRS})

        # start namespace for subdir
        get_filename_component(DIR_NAME ${SUBDIR} NAME)

        # gather all shader files from subdir
        get_shader_sources(GLSL_FOLDER_FILES ${SUBDIR})

        if (GLSL_FOLDER_FILES)

            message(STATUS "compiling shaders: ${DIR_NAME}")

            list(APPEND ALL_GLSL_SOURCES ${GLSL_FOLDER_FILES})

            # open namespace
            file(APPEND ${OUTPUT_HEADER} "\nnamespace ${DIR_NAME}\n{\n\n")
            file(APPEND ${OUTPUT_SOURCE} "\nnamespace ${DIR_NAME}\n{\n\n")
        endif ()

        foreach (GLSL ${GLSL_FOLDER_FILES})

            get_filename_component(FILE_NAME ${GLSL} NAME)
            string(REGEX REPLACE "[.]" "_" NAME ${FILE_NAME})
            set(SPIRV "${SPIRV_OUT_DIR}/shaders/${DIR_NAME}_${NAME}.spv")
            #                    message(${SPIRV})
            list(APPEND SPIRV_BINARY_FILES ${SPIRV})

            execute_process(
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUT_DIR}/shaders/"
                    COMMAND ${SHADER_COMPILER} --target-env ${SPIRV_TARGET_ENV} ${GLSL} -o ${SPIRV}
                    OUTPUT_VARIABLE glslang_std_out
                    ERROR_VARIABLE glslang_std_err
                    RESULT_VARIABLE ret
                    #                OUTPUT_QUIET
            )

            if (NOT "${ret}" STREQUAL "0")
                message(WARNING "Failed to compile shader: ${glslang_std_out}")
            endif ()

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

        if (GLSL_FOLDER_FILES)
            # close namespace
            file(APPEND ${OUTPUT_HEADER} "\n}// namespace ${DIR_NAME}\n")
            file(APPEND ${OUTPUT_SOURCE} "\n}// namespace ${DIR_NAME}\n")
        endif ()

    endforeach (SUBDIR)

    # close namespace
    file(APPEND ${OUTPUT_HEADER} "\n}// namespace ${TOP_NAMESPACE}\n")
    file(APPEND ${OUTPUT_SOURCE} "\n}// namespace ${TOP_NAMESPACE}\n")

endfunction(STRINGIFY_SHADERS)
