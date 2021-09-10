cmake_minimum_required(VERSION 3.8)

## define our project
project(vierkant)

## request C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wformat=0")

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

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)
option(BUILD_TESTS "Build Tests" ON)
option(BUILD_EXAMPLES "Build Examples" ON)

set(LIB_TYPE SHARED)
if (NOT BUILD_SHARED_LIBS)
    set(LIB_TYPE STATIC)
    set(Boost_USE_STATIC_LIBS ON)
endif (NOT BUILD_SHARED_LIBS)

## cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
include(add_test_helper)
include(vierkant_utils)

##### BOOST
set(BOOST_COMPONENTS system)
if (BUILD_TESTS)
    ENABLE_TESTING()
    set(BOOST_COMPONENTS ${BOOST_COMPONENTS} unit_test_framework)
    if (BUILD_SHARED_LIBS)
        add_definitions(-DBOOST_TEST_DYN_LINK)
    endif (BUILD_SHARED_LIBS)
endif (BUILD_TESTS)
find_package(Boost REQUIRED COMPONENTS ${BOOST_COMPONENTS})
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})
#####

## GLFW
find_package(glfw3 3.2 REQUIRED)
set(LIBS ${LIBS} glfw)

## Vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
SET(LIBS ${LIBS} ${Vulkan_LIBRARIES})

#crocore
add_subdirectory("crocore")
include_directories(${crocore_INCLUDE_DIRS})
set(LIBS ${LIBS} ${crocore_LIBRARIES})

#shaders
set(GLSLANG_VALIDATOR "glslangValidator")
stringify_shaders("shaders" "shaders" ${GLSLANG_VALIDATOR})

option(USE_ASSIMP OFF)

#### assimp
if (USE_ASSIMP)
    find_package(ASSIMP REQUIRED)
    include_directories(${ASSIMP_INCLUDE_DIRS})
    SET(LIBS ${LIBS} ${ASSIMP_LIBRARIES})
    add_definitions(-DUSE_ASSIMP)
else (USE_ASSIMP)
    remove_definitions(-DUSE_ASSIMP)
endif (USE_ASSIMP)

### assimp
#option(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
#option(ASSIMP_BUILD_SAMPLES OFF)
#option(ASSIMP_BUILD_TESTS OFF)
#option(ASSIMP_INSTALL OFF)
#add_subdirectory("assimp")
#include_directories(assimp/include)
#set(LIBS ${LIBS} assimp)

#tinygltf
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

set(TINYGLTF_NO_INCLUDE_STB_IMAGE "" FORCE)
set(TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE "" FORCE)

add_subdirectory("tinygltf")
include_directories("tinygltf")

#vierkant
add_subdirectory("src")
include_directories(${vierkant_INCLUDE_DIRS})
set(LIBS ${LIBS} ${vierkant_LIBRARIES})

# Expose public includes (including Boost transitively) to other
# subprojects through cache variable.
set(${PROJECT_NAME}_INCLUDE_DIRS
        ${crocore_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/3rdparty/include
        ${PROJECT_BINARY_DIR}/include
        CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

set(${PROJECT_NAME}_LIBRARIES ${PROJECT_NAME} ${crocore_LIBRARIES}
        CACHE INTERNAL "${PROJECT_NAME}: Libraries" FORCE)

# message("includes: ${vierkant_INCLUDE_DIRS}")
# message("libs: ${vierkant_LIBRARIES}")

#examples
if (BUILD_EXAMPLES)
    add_subdirectory("samples")
endif (BUILD_EXAMPLES)
