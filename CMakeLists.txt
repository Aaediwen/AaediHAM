cmake_minimum_required(VERSION 3.16...4.0)
project(AaediClock)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# -pedantic

set(SOURCES
    aaediclock.cc
    utils.cc
    modules.cc
    classes.cc
    modules/kindex.cc
    modules/dx_spots.cc
    modules/sat_tracker.cc
    modules/de_dx.cc
    modules/pota.cc
    modules/callsign.cc
    modules/map.cc
    modules/clock.cc
)

if(MSVC)
 #   add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Os)
endif()

add_executable(clock ${SOURCES})

if(WIN32)
    set(CMAKE_CXX_STANDARD 17)
    target_compile_definitions(clock PRIVATE _CRT_SECURE_NO_WARNINGS)
    set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Windows")
    list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_DIR}")
    add_subdirectory(${EXTERNAL_DIR}/libsgp4)
    target_include_directories(clock PRIVATE ${EXTERNAL_DIR}/ ${EXTERNAL_DIR}/json-develop/include)
    find_package(SDL3 REQUIRED)
    find_package(SDL3_ttf REQUIRED)
    set(SGP4_LIB sgp4)
    target_link_libraries(clock PRIVATE winhttp sgp4 SDL3::SDL3 SDL3_ttf::SDL3_ttf ${SGP4_LIB} )
elseif(APPLE)
    set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/macos")
    list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_DIR}")
    find_package(ZLIB REQUIRED)
    find_package(CURL REQUIRED)
    find_package(SDL3 REQUIRED)
    find_package(SDL3_ttf REQUIRED)
    set(SGP4_LIB sgp4s)
    target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl SDL3::SDL3 SDL3_ttf::SDL3_ttf ${SGP4_LIB} m)
else()
    set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Linux")
    list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_DIR}")
    find_package(ZLIB REQUIRED)
    find_package(CURL REQUIRED)
    find_package(SDL3 REQUIRED)
    find_package(SDL3_ttf REQUIRED)
    set(SGP4_LIB sgp4s)
    target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl SDL3::SDL3 SDL3_ttf::SDL3_ttf ${SGP4_LIB} m
    fontconfig)
endif()