set(LIB_NAME ${PROJECT_NAME})

FILE(GLOB FOLDER_SOURCES *.c* ${CMAKE_CURRENT_BINARY_DIR}/*.cpp)
FILE(GLOB FOLDER_HEADERS *.h*)

include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(${LIB_NAME} ${LIB_TYPE} ${FOLDER_SOURCES} ${FOLDER_HEADERS})
add_dependencies(${LIB_NAME} shaders)
target_link_libraries(${LIB_NAME} ${LIBS} crocore)
set(LIBS ${LIBS} ${LIB_NAME})

#Boost Unit-Tests
addTestMacro()

set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include ${crocore__INCLUDE_DIRS}
        CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)