set(TEST_APP "simple_test")

FILE(GLOB FOLDER_SOURCES *.c*)
FILE(GLOB FOLDER_HEADERS *.h*)

include_directories(${vierkant_INCLUDE_DIRS})
add_executable(${TEST_APP} ${FOLDER_SOURCES} ${FOLDER_HEADERS})
target_link_libraries(${TEST_APP} ${LIBS} crocore vierkant)


#Boost Unit-Tests
addTestMacro()