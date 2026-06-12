set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
include(vierkant_utils)

if (NOT DEFINED SLANG_SOURCE_ROOT)
    set(SLANG_SOURCE_ROOT "shaders/slang")
endif ()

if (NOT DEFINED TARGET_NAME)
    set(TARGET_NAME ${PROJECT_NAME})
endif ()

if (NOT DEFINED SPIRV_OUTPUT_DIR)
    set(SPIRV_OUTPUT_DIR "${PROJECT_BINARY_DIR}")
endif ()

if (NOT DEFINED SOURCE_OUTPUT_DIR)
    set(SOURCE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif ()

# set slang compiler (optional override, otherwise search PATH)
if (SLANGC_OVERRIDE)
    set(SLANG_COMPILER "${SLANGC_OVERRIDE}")
else ()
    find_program(SLANG_COMPILER "slangc" REQUIRED)
endif ()

stringify_slang_shaders(${SLANG_SOURCE_ROOT} ${TARGET_NAME} ${SLANG_COMPILER} ${SPIRV_OUTPUT_DIR} ${SOURCE_OUTPUT_DIR} ${SPIRV_DEBUG_SYMBOLS})
