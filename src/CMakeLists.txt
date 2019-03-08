set(LIB_NAME "vierkant")

FILE(GLOB FOLDER_SOURCES *.c* ${CMAKE_CURRENT_BINARY_DIR}/*.cpp)
FILE(GLOB FOLDER_HEADERS *.h*)

include_directories(${CMAKE_BINARY_DIR}/include)

add_library(${LIB_NAME} ${LIB_TYPE} ${FOLDER_SOURCES} ${FOLDER_HEADERS})
add_dependencies(${LIB_NAME} shaders)
target_link_libraries(${LIB_NAME} ${LIBS} crocore)
set(LIBS ${LIBS} ${LIB_NAME})

#Boost Unit-Tests
addTestMacro()

add_subdirectory("sample")