cmake_minimum_required(VERSION 3.16)
project(uapki_onec_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(UAPKI_ROOT "" CACHE PATH "Path to prebuilt UAPKI artifacts")

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

