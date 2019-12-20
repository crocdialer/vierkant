set(LIB_NAME ${PROJECT_NAME})

FILE(GLOB FOLDER_SOURCES *.c* imgui/*.c* ${CMAKE_CURRENT_BINARY_DIR}/*.cpp)
FILE(GLOB FOLDER_HEADERS *.h* ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME})

include_directories(${CMAKE_BINARY_DIR}/include ${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(${LIB_NAME} ${LIB_TYPE} ${FOLDER_SOURCES} ${FOLDER_HEADERS} Arcball.cpp ../include/vierkant/Arcball.hpp)
add_dependencies(${LIB_NAME} shaders)
target_link_libraries(${LIB_NAME} ${LIBS})
set(LIBS ${LIBS} ${LIB_NAME})

#Boost Unit-Tests
addTestMacro()

set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/include ${PROJECT_SOURCE_DIR}/include ${crocore_INCLUDE_DIRS}
        CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)