include_directories(${vierkant_INCLUDE_DIRS})

# internal headers, e.g. the platform gamepad-backends
include_directories(${PROJECT_SOURCE_DIR}/src)
set(LIBS ${vierkant_LIBRARIES} GTest::gtest_main)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # gcc requires additional linking against libatomic, msvc/clang do not provide it.
    set(LIBS ${LIBS} atomic)
endif ()

# test-only slang shaders, same build as the library's
get_slang_recursive(TEST_SLANG_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(TEST_SHADERS_CPP ${CMAKE_CURRENT_BINARY_DIR}/src/shaders_slang.cpp)

add_custom_command(OUTPUT ${TEST_SHADERS_CPP} ${CMAKE_CURRENT_BINARY_DIR}/include/test_shaders/shaders_slang.hpp
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -D SLANG_SOURCE_ROOT="tests/shaders"
        -D SPIRV_OUTPUT_DIR="${PROJECT_BINARY_DIR}"
        -D SOURCE_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        -D TARGET_NAME="test_shaders"
        -D SPIRV_DEBUG_SYMBOLS=${SPIRV_DEBUG_SYMBOLS}
        -D SLANGC_OVERRIDE=${SLANGC_OVERRIDE}
        -P cmake_modules/build_slang_shaders.cmake
        DEPENDS ${TEST_SLANG_SOURCE_FILES}
        COMMENT "recompiling test slang shaders -> SPIRV -> shaders_slang.hpp/cpp")

add_library(test_shaders STATIC ${TEST_SHADERS_CPP})
target_include_directories(test_shaders PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
set(LIBS ${LIBS} test_shaders)
FILE(GLOB TEST_SOURCES "*.c*")
FILE(GLOB TEST_HEADERS "*.h")

# the evdev-backend is linux-only. an empty translation-unit here links against libucrt instead of
# mimalloc on msvc and dies with LNK2005, so drop the file rather than guarding its contents.
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/TestGamepadEvdev.cpp")
endif ()

include(GoogleTest)

SOURCE_GROUP("Unit-Tests" FILES ${TEST_SOURCES} ${TEST_HEADERS})
FOREACH (test_file ${TEST_SOURCES})
    STRING(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" test_name ${test_file})
    STRING(REPLACE ".cpp" "" test_name ${test_name})
    add_executable(${test_name} ${test_file} ${TEST_HEADERS})
    TARGET_LINK_LIBRARIES(${test_name} ${LIBS})
    gtest_discover_tests(${test_name})
ENDFOREACH (test_file)
