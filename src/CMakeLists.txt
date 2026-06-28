# 3rd party header-only libs
set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/extern/imgui)

# suppress warnings from 3rd party headers
include_directories(SYSTEM ${IMGUI_DIR})

FILE(GLOB FOLDER_SOURCES *.c* imgui/*.c* model/*.c* ${CMAKE_CURRENT_BINARY_DIR}/*.cpp ${IMGUI_DIR}/*.c*)
FILE(GLOB FOLDER_HEADERS *.h* model/*.h* ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME}
        ${IMGUI_DIR}/*.h*)

# vendored submodule TUs - bc7enc_rdo core subset only (no rdo/ert/ispc)
set(VENDORED_SOURCES
        ${PROJECT_SOURCE_DIR}/extern/volk/volk.c
        ${PROJECT_SOURCE_DIR}/extern/bc7enc_rdo/bc7enc.cpp
        ${PROJECT_SOURCE_DIR}/extern/bc7enc_rdo/bc7decomp.cpp
        ${PROJECT_SOURCE_DIR}/extern/bc7enc_rdo/rgbcx.cpp)

if (NOT MSVC)
    set_source_files_properties(
            ${PROJECT_SOURCE_DIR}/extern/bc7enc_rdo/bc7enc.cpp
            ${PROJECT_SOURCE_DIR}/extern/bc7enc_rdo/bc7decomp.cpp
            ${PROJECT_SOURCE_DIR}/extern/bc7enc_rdo/rgbcx.cpp
            PROPERTIES COMPILE_OPTIONS "-w")
endif ()

include_directories(${PROJECT_BINARY_DIR}/include ${CMAKE_BINARY_DIR}/${LIB_NAME}/include ${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(${LIB_NAME} ${LIB_TYPE} ${FOLDER_SOURCES} ${FOLDER_HEADERS} ${VENDORED_SOURCES})
add_dependencies(${LIB_NAME} shaders slang_shaders)
target_link_libraries(${LIB_NAME} ${LIBS})