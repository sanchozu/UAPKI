cmake_minimum_required(VERSION 3.16)
project(uapki_onec_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(__MINGW_USE_VC2005_COMPAT)
endif ()

option(UAPKI_ONEC_EMBED "Build UAPKI from sources bundled with the repo" ON)
set(UAPKI_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../library" CACHE PATH "Path to UAPKI sources")
set(UAPKI_ROOT "" CACHE PATH "Path to prebuilt UAPKI artifacts")

if(UAPKI_ONEC_EMBED)
    if(NOT EXISTS "${UAPKI_SOURCE_ROOT}/uapki/CMakeLists.txt")
        message(FATAL_ERROR "Embedded build requested but ${UAPKI_SOURCE_ROOT} does not look like a UAPKI checkout")
    endif()
    # Make sure every static archive is built with -fPIC so it can be linked into the NativeAPI shared library.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(UAPKI_LIBS_TYPE STATIC CACHE STRING "Type of UAPKI libraries" FORCE)
    set(UAPKI_DISABLE_COPY ON CACHE BOOL "Disable file copying from embedded UAPKI" FORCE)
    # Export UAPKI public headers so that uapki-export.h is reachable when we link to the target.
    set(UAPKI_PUBLIC_INTERNAL_INCLUDES ON CACHE BOOL "Expose UAPKI internal headers" FORCE)

    add_subdirectory(${UAPKI_SOURCE_ROOT}/uapkic ${CMAKE_BINARY_DIR}/uapkic)
    add_subdirectory(${UAPKI_SOURCE_ROOT}/uapkif ${CMAKE_BINARY_DIR}/uapkif)
    add_subdirectory(${UAPKI_SOURCE_ROOT}/uapki ${CMAKE_BINARY_DIR}/uapki)
    set(UAPKI_LIBRARY uapki)
    set(UAPKI_INCLUDE_DIR "")
else()
    find_path(UAPKI_INCLUDE_DIR uapki-export.h
        HINTS
            ${UAPKI_ROOT}/include
            ${CMAKE_SOURCE_DIR}/../../library/uapki/include
        NO_DEFAULT_PATH)
    if(NOT UAPKI_INCLUDE_DIR)
        find_path(UAPKI_INCLUDE_DIR uapki-export.h)
    endif()

    find_library(UAPKI_LIBRARY NAMES uapki
        HINTS
            ${UAPKI_ROOT}/lib
            ${CMAKE_SOURCE_DIR}/../../library/build/out
            ${CMAKE_SOURCE_DIR}/../../library/out
    )

    if(NOT UAPKI_INCLUDE_DIR)
        message(FATAL_ERROR "Could not locate uapki-export.h. Set UAPKI_ROOT or UAPKI_INCLUDE_DIR.")
    endif()

    if(NOT UAPKI_LIBRARY)
        message(FATAL_ERROR "Could not locate uapki library. Set UAPKI_ROOT or UAPKI_LIBRARY.")
    endif()
endif()

add_library(uapki_onec_native SHARED
    src/base64.cpp
    src/entry.cpp
    src/uapki_component.cpp
    src/uapki_json_client.cpp
    src/variant_utils.cpp
)

set_target_properties(uapki_onec_native PROPERTIES
    OUTPUT_NAME "uapki1c"
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(uapki_onec_native
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${UAPKI_INCLUDE_DIR}
)

target_link_libraries(uapki_onec_native
    PRIVATE
        ${UAPKI_LIBRARY}
)

install(TARGETS uapki_onec_native
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

