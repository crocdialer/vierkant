include_directories(${vierkant_INCLUDE_DIRS})
set(LIBS ${vierkant_LIBRARIES} GTest::gtest_main)

FILE(GLOB TEST_SOURCES "*.c*")
FILE(GLOB TEST_HEADERS "*.h")

include(GoogleTest)

SOURCE_GROUP("Unit-Tests" FILES ${TEST_SOURCES} ${TEST_HEADERS})
FOREACH (test_file ${TEST_SOURCES})
    STRING(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" test_name ${test_file})
    STRING(REPLACE ".cpp" "" test_name ${test_name})
    add_executable(${test_name} ${test_file} ${TEST_HEADERS})
    TARGET_LINK_LIBRARIES(${test_name} ${LIBS})
    gtest_discover_tests(${test_name})
ENDFOREACH(test_file)
