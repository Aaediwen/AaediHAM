cmake_minimum_required(VERSION 3.16...4.0)
project(AaediClock)
include(FetchContent)
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
    modules/ncdxf.cc
#    modules/sun.cc
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
    set(FETCHCONTENT_BASE_DIR ${EXTERNAL_DIR})
    list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_DIR}")
    find_package(ZLIB REQUIRED)
    find_package(CURL)
    if (NOT CURL_FOUND)
    	message(STATUS "libcurl not found, fetching...")
	FetchContent_Declare(
  		curl
  		GIT_REPOSITORY https://github.com/curl/curl.git
  		PREFIX ${EXTERNAL_DIR}/curl
                GIT_TAG master
                
	)
	FetchContent_MakeAvailable(curl)
    else()
    	message(STATUS "using system libcurl ")
    endif()
    find_package(SDL3)
    if (NOT SDL3_FOUND)
    	message(STATUS "SDL3 not found, fetching...")
    # Disable warnings as errors globally for SDL3
	FetchContent_Declare(
  		SDL3
  		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  		PREFIX ${EXTERNAL_DIR}/sdl3
                GIT_TAG release-3.2.18
                
	)
	FetchContent_MakeAvailable(SDL3)
    else()
    	message(STATUS "using system SDL3 ")
    endif()
    find_package(SDL3_ttf)
    if (NOT SDL3_ttf_FOUND)
    	message(STATUS "SDL3 TTF not found, fetching...")
	FetchContent_Declare(
  		SDL3_ttf
  		GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  		PREFIX ${EXTERNAL_DIR}/sdl3_ttf
                GIT_TAG release-3.2.2
                
	)
	FetchContent_MakeAvailable(SDL3_ttf)
    else()
    	message(STATUS "using system SDL3 TTF")
    endif()

    	message(STATUS "SGP4 not found, fetching...")
	FetchContent_Declare(
  		sgp4
  		GIT_REPOSITORY https://github.com/dnwrnr/sgp4.git
  		PREFIX ${EXTERNAL_DIR}/sgp4
                GIT_TAG master
                
	)
	FetchContent_MakeAvailable(sgp4)

    set(SGP4_LIB sgp4s)
    
   FetchContent_Declare(
   	json
  	GIT_REPOSITORY https://github.com/nlohmann/json.git
  	PREFIX ${EXTERNAL_DIR}/json
        GIT_TAG master
                
	)
	FetchContent_MakeAvailable(json)


    target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl SDL3::SDL3 SDL3_ttf::SDL3_ttf ${SGP4_LIB} m
    fontconfig jpeg)
endif()