set(TEST_APP "simple_test")

FILE(GLOB FOLDER_SOURCES *.c*)
FILE(GLOB FOLDER_HEADERS *.h*)

add_executable(${TEST_APP} ${FOLDER_SOURCES} ${FOLDER_HEADERS})
target_link_libraries(${TEST_APP} ${LIBS})


#Boost Unit-Tests
addTestMacro()