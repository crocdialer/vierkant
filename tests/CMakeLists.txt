include_directories(${CMAKE_BINARY_DIR}/include ${CMAKE_BINARY_DIR}/${LIB_NAME}/include ${PROJECT_SOURCE_DIR}/include)

FILE(GLOB TEST_SOURCES "*.c*")
FILE(GLOB TEST_HEADERS "*.h")

SOURCE_GROUP("Unit-Tests" FILES ${TEST_SOURCES} ${TEST_HEADERS})
FOREACH(testFile ${TEST_SOURCES})
    STRING(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" testName ${testFile})
    STRING(REPLACE ".cpp" "" testName ${testName})
    add_executable(${testName} ${testFile} ${TEST_HEADERS})
    TARGET_LINK_LIBRARIES(${testName} ${LIBS})
    add_test(${testName} "${EXECUTABLE_OUTPUT_PATH}/${testName}")
#                message(STATUS "found unit-test: ${testName}")
ENDFOREACH(testFile)