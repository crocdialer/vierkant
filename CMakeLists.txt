cmake_minimum_required(VERSION 3.16)

## define our project
project(vierkant)

# retrieve git-commit hash/date
find_program(GIT_BINARY git)

execute_process(
        COMMAND ${GIT_BINARY} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
        COMMAND ${GIT_BINARY} log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
        COMMAND ${GIT_BINARY} log -1 --format=%cd --date=local
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/vierkant/git_hash.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/include/vierkant/git_hash.h" @ONLY)

## request C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
endif (NOT CMAKE_BUILD_TYPE)

set(RELEASE_BUILDS "Release MinSizeRel")

if (${CMAKE_BUILD_TYPE} IN_LIST RELEASE_BUILDS)
    list(APPEND CXX_FLAGS "-O3")
    add_definitions(-DNDEBUG)
else ()
    remove_definitions(-DNDEBUG)
endif ()

option(BUILD_SHARED_LIBS "Build Shared Libraries" OFF)
option(BUILD_TESTS "Build Tests" ON)
option(BUILD_EXAMPLES "Build Examples" ON)
option(BUILD_SPIRV_DEBUG_SYMBOLS "Build SPIRV debug symbols" OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
endif ()

if (NOT BUILD_SHARED_LIBS)
    set(CMAKE_CXX_FLAGS ${CXX_DEFAULT_FLAGS})
    set(LIB_TYPE STATIC)
else ()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(LIB_TYPE SHARED)
endif (NOT BUILD_SHARED_LIBS)

if (BUILD_SPIRV_DEBUG_SYMBOLS)
    set(SPIRV_DEBUG_SYMBOLS ON CACHE INTERNAL "")
else ()
    set(SPIRV_DEBUG_SYMBOLS OFF CACHE INTERNAL "")
endif ()

## cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
include(vierkant_utils)

set(INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/vierkant/bundled
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/extern/imgui)

# suppress warnings from bundles headers
include_directories(BEFORE SYSTEM "${PROJECT_SOURCE_DIR}/include/vierkant/bundled")
include_directories(BEFORE SYSTEM "${PROJECT_SOURCE_DIR}/extern/imgui")

# mimalloc
set(MI_BUILD_TESTS OFF)
add_subdirectory(extern/mimalloc EXCLUDE_FROM_ALL)
include_directories(BEFORE SYSTEM ${mimalloc_INCLUDE_DIRS})
set(LIBS ${LIBS} mimalloc-static)

# vulkan/volk
find_package(Vulkan)
include_directories(${Vulkan_INCLUDE_DIRS})
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})

# glfw
add_subdirectory(extern/glfw EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} glfw)

# glm
add_subdirectory(extern/glm EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} glm)

# tinygltf/tinyobj, suppress warnings from 3rd party headers
include_directories(BEFORE SYSTEM "extern/tinygltf")
include_directories(BEFORE SYSTEM "extern/tinyobjloader")

# zeux/meshoptimizer
add_subdirectory(extern/meshoptimizer EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} meshoptimizer)

# SPIRV-Reflect -> libspirv-reflect-static.a
set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(extern/SPIRV-Reflect EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} spirv-reflect-static)

#crocore
add_subdirectory("extern/crocore")
include_directories(${crocore_INCLUDE_DIRS})
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${crocore_INCLUDE_DIRS})
set(LIBS ${LIBS} ${crocore_LIBRARIES})

# entt
add_subdirectory(extern/entt EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} EnTT::EnTT)

# JoltPhysics
set(GENERATE_DEBUG_SYMBOLS OFF)
set(CPP_RTTI_ENABLED ON)
set(DEBUG_RENDERER_IN_DISTRIBUTION ON)
add_subdirectory("extern/JoltPhysics/Build" EXCLUDE_FROM_ALL)
include_directories(SYSTEM extern/JoltPhysics/Jolt)
set(LIBS ${LIBS} Jolt)

##shaders
include(build_shaders)
get_shaders_recursive(GLSL_SOURCE_FILES "shaders")
set(SHADERS_CPP ${CMAKE_CURRENT_BINARY_DIR}/src/shaders.cpp)

# shaders are automatically recompiled if changed
add_custom_command(OUTPUT ${SHADERS_CPP}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -D SHADER_SOURCE_ROOT="shaders"
        -D SPIRV_OUTPUT_DIR="${PROJECT_BINARY_DIR}"
        -D SOURCE_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        -D TARGET_NAME="${PROJECT_NAME}"
        -D SPIRV_DEBUG_SYMBOLS=${SPIRV_DEBUG_SYMBOLS}
        -P cmake_modules/build_shaders.cmake
        DEPENDS ${GLSL_SOURCE_FILES}
        COMMENT "recompiling shaders -> SPIRV -> shaders.hpp/cpp")

add_custom_target("shaders" DEPENDS ${SHADERS_CPP})

# vierkant
set(LIB_NAME ${PROJECT_NAME})
add_subdirectory("src")

if (MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /W2 /WX) # /W4
else ()
    target_compile_options(mimalloc-static PRIVATE -Wall -Wextra -Werror -Wno-unused-function)
    target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra -Werror) # -Wpedantic
endif ()

# Expose public includes to subprojects through cache variable.
set(${PROJECT_NAME}_INCLUDE_DIRS ${INCLUDE_DIRS}
        CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

set(${PROJECT_NAME}_LIBRARIES ${PROJECT_NAME}
        CACHE INTERNAL "${PROJECT_NAME}: Libraries" FORCE)

#examples
if (BUILD_EXAMPLES)
    add_subdirectory(samples)
endif (BUILD_EXAMPLES)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif (BUILD_TESTS)

# install vierkant-headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/${LIB_NAME} DESTINATION "include")
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/${LIB_NAME} DESTINATION "include")

# install entt-headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/extern/entt/single_include/entt DESTINATION "include/vierkant/bundled")

# install glm-headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/extern/glm/glm DESTINATION "include/vierkant/bundled")

# install imgui-headers
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui)
install(FILES ${IMGUI_DIR}/imconfig.h ${IMGUI_DIR}/imgui.h DESTINATION "include/vierkant/bundled")

install(TARGETS ${LIB_NAME} DESTINATION lib)
