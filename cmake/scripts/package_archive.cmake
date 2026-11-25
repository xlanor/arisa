cmake_minimum_required(VERSION 3.28)

foreach(required STAGE NAME OUTPUT_DIR STRIP)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "package_archive.cmake needs -D${required}=")
    endif()
endforeach()

file(REAL_PATH "${STAGE}" STAGE)
get_filename_component(OUTPUT_DIR "${OUTPUT_DIR}" ABSOLUTE)

if(NOT IS_DIRECTORY "${STAGE}/${NAME}")
    message(FATAL_ERROR "${STAGE}/${NAME} is not an installed package")
endif()

set(debug_name "${NAME}-debug")
file(REMOVE_RECURSE "${STAGE}/${debug_name}")
file(COPY "${STAGE}/${NAME}/" DESTINATION "${STAGE}/${debug_name}")

execute_process(
    COMMAND ${STRIP} "${STAGE}/${NAME}/lib/libarisa.a"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "could not strip the debug info from ${STAGE}/${NAME}/lib/libarisa.a")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(package IN ITEMS "${NAME}" "${debug_name}")
    set(archive "${OUTPUT_DIR}/${package}.tar.gz")
    file(REMOVE "${archive}" "${archive}.sha256")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar czf "${archive}" --format=gnutar --mtime=1970-01-01 "${package}"
        WORKING_DIRECTORY "${STAGE}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "could not archive ${STAGE}/${package}")
    endif()
    file(SHA256 "${archive}" digest)
    file(WRITE "${archive}.sha256" "${digest}  ${package}.tar.gz\n")
    file(SIZE "${archive}" size)
    message(STATUS "${archive} ${size} bytes")
    message(STATUS "sha256 ${digest}")
endforeach()
