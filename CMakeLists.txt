cmake_minimum_required(VERSION 3.12)

## define our project
project(vierkant)

# retrieve git-commit hash/date
find_program(GIT_BINARY git)
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

if (NOT BUILD_SHARED_LIBS)
    set(CMAKE_CXX_FLAGS ${CXX_DEFAULT_FLAGS})
    set(LIB_TYPE STATIC)
else ()
    set(CMAKE_CXX_FLAGS "${CXX_DEFAULT_FLAGS} -fPIC")
    set(LIB_TYPE SHARED)
endif (NOT BUILD_SHARED_LIBS)

## cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
include(vierkant_utils)

set(INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/3rdparty/include
        ${PROJECT_BINARY_DIR}/include)

# vulkan/volk
find_package(Vulkan)
include_directories(${Vulkan_INCLUDE_DIRS})

# glfw
add_subdirectory(submodules/glfw EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} glfw)

# glm
add_subdirectory(submodules/glm EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} glm)

# tinygltf/tinyobj, suppress warnings from 3rd party headers
include_directories(BEFORE SYSTEM "submodules/tinygltf")
include_directories(BEFORE SYSTEM "submodules/tinyobjloader")

# zeux/meshoptimizer
add_subdirectory(submodules/meshoptimizer EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} meshoptimizer)

# vma
add_subdirectory(submodules/VulkanMemoryAllocator EXCLUDE_FROM_ALL)
include_directories(SYSTEM submodules/VulkanMemoryAllocator/include)
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/submodules/VulkanMemoryAllocator/include)

#crocore
add_subdirectory("submodules/crocore")
include_directories(${crocore_INCLUDE_DIRS})
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${crocore_INCLUDE_DIRS})
set(LIBS ${LIBS} ${crocore_LIBRARIES})

# entt
add_subdirectory(submodules/entt EXCLUDE_FROM_ALL)
set(LIBS ${LIBS} EnTT::EnTT)

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
        -P cmake_modules/build_shaders.cmake
        DEPENDS ${GLSL_SOURCE_FILES}
        COMMENT "recompiling shaders -> SPIRV -> shaders.hpp/cpp")

add_custom_target("shaders" DEPENDS ${SHADERS_CPP})

# vierkant
set(LIB_NAME ${PROJECT_NAME})
add_subdirectory("src")
set(LIBS ${LIBS} ${vierkant_LIBRARIES})

if (MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /W2 /WX) # /W4
else ()
    target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra -Werror) # -Wpedantic
endif ()

# Expose public includes to subprojects through cache variable.
set(${PROJECT_NAME}_INCLUDE_DIRS ${INCLUDE_DIRS}
        CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

set(${PROJECT_NAME}_LIBRARIES ${PROJECT_NAME} ${crocore_LIBRARIES}
        CACHE INTERNAL "${PROJECT_NAME}: Libraries" FORCE)

#examples
if (BUILD_EXAMPLES)
    add_subdirectory(samples)
endif (BUILD_EXAMPLES)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif (BUILD_TESTS)
