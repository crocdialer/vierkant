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

# --- slang helpers -------------------------------------------------------
# recursive collection of `.slang` source files mirroring GET_SHADERS_RECURSIVE
function(GET_SLANG_RECURSIVE RESULT SLANG_FOLDER)

    # search subdirs
    subdirlist(SUBDIRS ${SLANG_FOLDER})
    # set(SUBDIRS "${SLANG_FOLDER}")

    foreach (SUBDIR ${SUBDIRS})
        file(GLOB SLANG_FOLDER_FILES "${SUBDIR}/*.slang")
        list(APPEND ALL_SOURCES ${SLANG_FOLDER_FILES})
    endforeach()

    set(${RESULT} ${ALL_SOURCES} PARENT_SCOPE)
endfunction(GET_SLANG_RECURSIVE)

# similar to STRINGIFY_SHADERS but uses slangc and produces a separate header/source pair
function(STRINGIFY_SLANG_SHADERS SLANG_FOLDER TARGET_NAME SLANG_COMPILER SPIRV_OUT_DIR SOURCE_OUT_DIR SPIRV_DEBUG_SYMBOLS)

    # SPIR-V generation with Slang
    set(SLANG_TARGET "spirv")

    # use VK_KHR_shader_non_semantic_info
    message("SPIRV_DEBUG_SYMBOLS: ${SPIRV_DEBUG_SYMBOLS}")
    if (SPIRV_DEBUG_SYMBOLS)
        set(SLANG_EXTRA_PARAMS "-g")
    endif ()

    # the top-level namespace for slang-generated blobs
    set(TOP_NAMESPACE "vierkant::slang_shaders")

    # remove existing spirv files in this branch
    file(GLOB SPIRV_FILES "${SPIRV_OUT_DIR}/${SLANG_FOLDER}/*.spv")
    if (SPIRV_FILES)
        file(REMOVE "${SPIRV_FILES}")
    endif ()

    set(OUTPUT_HEADER "${SOURCE_OUT_DIR}/include/${TARGET_NAME}/shaders_slang.hpp")
    set(OUTPUT_SOURCE "${SOURCE_OUT_DIR}/src/shaders_slang.cpp")

    # cleanup leftovers
    remove(${OUTPUT_HEADER})
    remove(${OUTPUT_SOURCE})

    # create output implementation and header
    file(WRITE ${OUTPUT_HEADER}
            "/* Generated file, do not edit! */\n\n"
            "#pragma once\n\n"
            "#include <array>\n\n"
            "namespace ${TOP_NAMESPACE}\n{\n\n")
    file(WRITE ${OUTPUT_SOURCE}
            "/* Generated file, do not edit! */\n\n"
            "#include \"${TARGET_NAME}/shaders_slang.hpp\"\n\n"
            "namespace ${TOP_NAMESPACE}\n{\n")

    # explore subdirectories under the given folder
    # subdirlist(SUBDIRS ${SLANG_FOLDER})
    set(SUBDIRS "${SLANG_FOLDER}")

    foreach (SUBDIR ${SUBDIRS})
        get_filename_component(DIR_NAME ${SUBDIR} NAME)

        file(GLOB SLANG_FILES "${SUBDIR}/*.slang")

        if (SLANG_FILES)
            message(STATUS "compiling slang shaders: ${DIR_NAME}")
            list(APPEND ALL_SLANG_SOURCES ${SLANG_FILES})

            file(APPEND ${OUTPUT_HEADER} "\nnamespace ${DIR_NAME}\n{\n\n")
            file(APPEND ${OUTPUT_SOURCE} "\nnamespace ${DIR_NAME}\n{\n\n")
        endif()

        foreach (SLANG ${SLANG_FILES})
            get_filename_component(FILE_NAME ${SLANG} NAME)
            string(REGEX REPLACE "[.]" "_" NAME ${FILE_NAME})
            set(SPIRV "${SPIRV_OUT_DIR}/${SLANG_FOLDER}/${DIR_NAME}_${NAME}.spv")
            list(APPEND SPIRV_BINARY_FILES ${SPIRV})

            execute_process(
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUT_DIR}/${SLANG_FOLDER}/"
                    COMMAND ${SLANG_COMPILER} -target ${SLANG_TARGET} ${SLANG_EXTRA_PARAMS} ${SLANG} -o ${SPIRV}
                    OUTPUT_VARIABLE slang_std_out
                    ERROR_VARIABLE slang_std_err
                    RESULT_VARIABLE ret
            )
            if (NOT "${ret}" STREQUAL "0")
                message(WARNING "Failed to compile slang shader: ${slang_std_err}")
            endif()

            # only process file if output was generated
            if (EXISTS "${SPIRV}")
                file(READ "${SPIRV}" contents HEX)
                string(LENGTH "${contents}" contents_length)
                math(EXPR num_bytes "${contents_length} / 2")

                file(APPEND ${OUTPUT_HEADER} "extern const std::array<unsigned char, ${num_bytes}> ${NAME};\n")
                file(APPEND ${OUTPUT_SOURCE} "\nconst std::array<unsigned char, ${num_bytes}> ${NAME} = {")

                string(REGEX MATCHALL ".." hex_values "${contents}")
                string(REPLACE ";" ", 0x" hex_values "${hex_values}")

                file(APPEND ${OUTPUT_SOURCE} "0x${hex_values}};\n")
            else()
                message(STATUS "skipping slang source without spirv output: ${SLANG}")
            endif()
        endforeach()

        if (SLANG_FILES)
            file(APPEND ${OUTPUT_HEADER} "\n}// namespace ${DIR_NAME}\n")
            file(APPEND ${OUTPUT_SOURCE} "\n}// namespace ${DIR_NAME}\n")
        endif()
    endforeach()

    file(APPEND ${OUTPUT_HEADER} "\n}// namespace ${TOP_NAMESPACE}\n")
    file(APPEND ${OUTPUT_SOURCE} "\n}// namespace ${TOP_NAMESPACE}\n")
endfunction(STRINGIFY_SLANG_SHADERS)

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

function(STRINGIFY_SHADERS GLSL_FOLDER TARGET_NAME SHADER_COMPILER SPIRV_OUT_DIR SOURCE_OUT_DIR SPIRV_DEBUG_SYMBOLS)

    # NOTE: seeing some issues with spirv-reflect/local-sizes between 1.2<->1.3
    set(SPIRV_TARGET_ENV vulkan1.3)

    # use VK_KHR_shader_non_semantic_info
    message("SPIRV_DEBUG_SYMBOLS: ${SPIRV_DEBUG_SYMBOLS}")
    if (SPIRV_DEBUG_SYMBOLS)
        set(GLSLANG_EXTRA_PARAMS "-gVS")
    endif ()

    set(TOP_NAMESPACE "vierkant::shaders")

    # remove existing spirv files
    file(GLOB SPIRV_FILES "${SPIRV_OUT_DIR}/${GLSL_FOLDER}/*.spv")

    if (SPIRV_FILES)
        file(REMOVE "${SPIRV_FILES}")
    endif ()

    set(OUTPUT_HEADER "${SOURCE_OUT_DIR}/include/${TARGET_NAME}/shaders.hpp")
    set(OUTPUT_SOURCE "${SOURCE_OUT_DIR}/src/shaders.cpp")

    # cleanup leftovers
    remove(${OUTPUT_HEADER})
    remove(${OUTPUT_SOURCE})
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
            set(SPIRV "${SPIRV_OUT_DIR}/${GLSL_FOLDER}/${DIR_NAME}_${NAME}.spv")
            #                    message(${SPIRV})
            list(APPEND SPIRV_BINARY_FILES ${SPIRV})

            execute_process(
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUT_DIR}/${GLSL_FOLDER}/"
                    COMMAND ${SHADER_COMPILER} --target-env ${SPIRV_TARGET_ENV} ${GLSLANG_EXTRA_PARAMS} ${GLSL} -o ${SPIRV}
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
