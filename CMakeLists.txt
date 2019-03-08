cmake_minimum_required(VERSION 3.3)

## define our project
project(vierkant)

## request C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wformat=0")

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)
option(BUILD_TESTS "Build Tests" ON)

set(LIB_TYPE SHARED)
if (NOT BUILD_SHARED_LIBS)
    set(LIB_TYPE STATIC)
endif (NOT BUILD_SHARED_LIBS)

## cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
include(add_test_helper)
include(vierkant_utils)

## GLFW
find_package(GLFW3 REQUIRED)
include_directories(${GLFW_INCLUDE_DIRS})
SET(LIBS ${LIBS} ${GLFW_LIBRARIES})

## Vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
SET(LIBS ${LIBS} ${Vulkan_LIBRARIES})

##### BOOST
set(Boost_USE_STATIC_LIBS ON)
set(BOOST_COMPONENTS system filesystem)
if (BUILD_TESTS)
    ENABLE_TESTING()
    set(BOOST_COMPONENTS ${BOOST_COMPONENTS} unit_test_framework)
endif (BUILD_TESTS)
find_package(Boost REQUIRED COMPONENTS ${BOOST_COMPONENTS})
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})
#####

#crocore
add_subdirectory("crocore")
include_directories(${crocore_INCLUDE_DIRS})

#shaders
stringify_shaders("shaders" "glslangValidator")

#vierkant
add_subdirectory("src")

#sample
add_subdirectory("sample")
