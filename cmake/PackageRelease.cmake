if(NOT DEFINED TEXNX_NRO OR NOT EXISTS "${TEXNX_NRO}")
    message(FATAL_ERROR "TexNX.nro is missing: ${TEXNX_NRO}")
endif()
if(NOT DEFINED TEXNX_RELEASE_DIR OR TEXNX_RELEASE_DIR STREQUAL "")
    message(FATAL_ERROR "TEXNX_RELEASE_DIR is not set")
endif()

set(TEXNX_RELEASE_STAGING "${TEXNX_RELEASE_DIR}/staging")
set(TEXNX_RELEASE_ZIP "${TEXNX_RELEASE_DIR}/TexNX.zip")

# ZIPに余計なdummy fileを入れず、空のTextures directoryをentryとして保持する。
file(REMOVE_RECURSE "${TEXNX_RELEASE_STAGING}")
file(MAKE_DIRECTORY "${TEXNX_RELEASE_STAGING}/switch/TexNX/Textures")
file(COPY "${TEXNX_NRO}" DESTINATION "${TEXNX_RELEASE_STAGING}/switch")
file(REMOVE "${TEXNX_RELEASE_ZIP}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${TEXNX_RELEASE_ZIP}"
        --format=zip -- switch
    WORKING_DIRECTORY "${TEXNX_RELEASE_STAGING}"
    RESULT_VARIABLE archiveResult
    OUTPUT_VARIABLE archiveOutput
    ERROR_VARIABLE archiveError
)
if(NOT archiveResult EQUAL 0 OR NOT EXISTS "${TEXNX_RELEASE_ZIP}")
    message(FATAL_ERROR
        "Failed to create TexNX.zip (${archiveResult})\n"
        "${archiveOutput}${archiveError}")
endif()

file(REMOVE_RECURSE "${TEXNX_RELEASE_STAGING}")
message(STATUS "Created ${TEXNX_RELEASE_ZIP}")
