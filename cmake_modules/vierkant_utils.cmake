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

# --- slang helpers -------------------------------------------------------
# recursive collection of `.slang` source files
function(GET_SLANG_RECURSIVE RESULT SLANG_FOLDER)

    # search subdirs, plus the root itself: it holds the entry-points (raypipeline, denoise, ...),
    # which would otherwise be missing from the recompile-rule's DEPENDS
    subdirlist(SUBDIRS ${SLANG_FOLDER})
    list(APPEND SUBDIRS "${SLANG_FOLDER}")

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
    set(SUBDIRS "${SLANG_FOLDER};${SLANG_FOLDER}/unlit;${SLANG_FOLDER}/fullscreen;${SLANG_FOLDER}/pbr")

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
                    COMMAND ${SLANG_COMPILER} -target ${SLANG_TARGET} -profile spirv_1_6 ${SLANG_EXTRA_PARAMS} ${SLANG} -o ${SPIRV}
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
