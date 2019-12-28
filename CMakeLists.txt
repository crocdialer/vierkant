cmake_minimum_required(VERSION 3.3)

## define our project
project(vierkant)

## request C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wformat=0")

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(RELEASE_BUILDS "Release MinSizeRel")
if(${CMAKE_BUILD_TYPE} IN_LIST RELEASE_BUILDS)
    list(APPEND CXX_FLAGS "-O3")
endif()

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)
option(BUILD_TESTS "Build Tests" ON)

set(LIB_TYPE SHARED)
if (NOT BUILD_SHARED_LIBS)
    set(LIB_TYPE STATIC)
    set(Boost_USE_STATIC_LIBS ON)
endif (NOT BUILD_SHARED_LIBS)

## cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
include(add_test_helper)
include(vierkant_utils)

##### BOOST
set(BOOST_COMPONENTS system filesystem)
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
find_package(glfw3 3.3 REQUIRED)
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
stringify_shaders("shaders" ${GLSLANG_VALIDATOR})

#vierkant
add_subdirectory("src")

#sample
add_subdirectory("sample")
