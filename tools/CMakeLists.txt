include_directories(${vierkant_INCLUDE_DIRS})

# cxxopts, used by the command-line tools only
include_directories(BEFORE SYSTEM "${PROJECT_SOURCE_DIR}/extern/cxxopts/include")

# the serialization-headers are internal to vierkant, reached like tests/ reach the platform
# backends. only vierkant_ed needs them - it archives its own documents to JSON.
set(VIERKANT_INTERNAL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src)

FILE(GLOB FOLDER_CONTENT *)

FOREACH (folderItem ${FOLDER_CONTENT})
    IF (IS_DIRECTORY ${folderItem})
        STRING(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" itemName ${folderItem})
        MESSAGE("adding tool: '${itemName}'")

        FILE(GLOB FOLDER_SOURCES ${folderItem}/*.c*)
        FILE(GLOB FOLDER_HEADERS ${folderItem}/*.h*)

        add_executable(${itemName} ${FOLDER_SOURCES} ${FOLDER_HEADERS})

        if (MSVC)
            target_compile_options(${itemName} PRIVATE /W2 /bigobj) #TODO /W4 /WX (warnings as errors)
        else ()
            target_compile_options(${itemName} PRIVATE -Wall -Wextra -Werror) # -Wpedantic
            # strip debug sections in Release (GCC 15/16 LTO emits DWARF at link-time by default)
            target_link_options(${itemName} PRIVATE $<$<CONFIG:Release>:-s>)
        endif ()

        target_link_libraries(${itemName} ${vierkant_LIBRARIES})
    ENDIF ()
ENDFOREACH (folderItem)

# vierkant_ed serializes engine components into its own scene/settings documents, so it needs the
# internal serialization-headers and cereal. the other tools use the cereal-free bundle.hpp.
target_include_directories(vierkant_ed PRIVATE ${VIERKANT_INTERNAL_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/extern/cereal/include)
