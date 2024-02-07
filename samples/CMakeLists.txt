# add our samples(s)
FILE(GLOB FOLDER_CONTENT *)

include_directories(${vierkant_INCLUDE_DIRS})

set(LIBS ${vierkant_LIBRARIES})
message(sample-libs: ${LIBS})

FOREACH(folderItem ${FOLDER_CONTENT})
    IF(IS_DIRECTORY ${folderItem})

        STRING(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" itemName ${folderItem})

        if(NOT(${itemName} STREQUAL ".git"))
            MESSAGE("adding sample: '${itemName}'")

            # include optional CMakeLists.txt in samples folder
             FILE(GLOB CMAKELISTS_EXIST "${folderItem}/CMakeLists.txt")

            if(CMAKELISTS_EXIST)
                message("using CMakeLists.txt for sample '${itemName}'")
                add_subdirectory(${folderItem})

            else(CMAKELISTS_EXIST)
                FILE(GLOB FOLDER_SOURCES ${folderItem}/*.c*)
                FILE(GLOB FOLDER_HEADERS ${folderItem}/*.h*)

                add_executable(${itemName} ${FOLDER_SOURCES} ${FOLDER_HEADERS})
                target_link_libraries(${itemName} ${LIBS})
            endif()
        endif()
    ENDIF()
ENDFOREACH(folderItem)
