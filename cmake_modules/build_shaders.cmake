set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
include(vierkant_utils)

if (NOT DEFINED SHADER_SOURCE_ROOT)
    set(SHADER_SOURCE_ROOT "shaders")
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

if (NOT DEFINED SPIRV_DEBUG_SYMBOLS)
    set(SPIRV_DEBUG_SYMBOLS OFF)
endif ()

# set glsl-compiler
find_program(SHADER_COMPILER "glslang" REQUIRED)

stringify_shaders(${SHADER_SOURCE_ROOT} ${TARGET_NAME} ${SHADER_COMPILER} ${SPIRV_OUTPUT_DIR} ${SOURCE_OUTPUT_DIR} ${SPIRV_DEBUG_SYMBOLS})