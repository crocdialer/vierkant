# 3rd party header-only libs
set(3RDPARTY_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/3rdparty/include)

# suppress warnings from 3rd party headers
include_directories(SYSTEM ${3RDPARTY_INCLUDE_DIR})

FILE(GLOB FOLDER_SOURCES *.c* imgui/*.c* bc7enc/*.c* model/*.c* ${CMAKE_CURRENT_BINARY_DIR}/*.cpp)
FILE(GLOB FOLDER_HEADERS *.h* model/*.h* ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME}
        ${3RDPARTY_INCLUDE_DIR}/${PROJECT_NAME})

include_directories(${PROJECT_BINARY_DIR}/include ${CMAKE_BINARY_DIR}/${LIB_NAME}/include ${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(${LIB_NAME} ${LIB_TYPE} ${FOLDER_SOURCES} ${FOLDER_HEADERS})
add_dependencies(${LIB_NAME} shaders)
target_link_libraries(${LIB_NAME} ${LIBS})