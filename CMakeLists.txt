cmake_minimum_required(VERSION 3.16...4.0)
project(AaediClock)
include(FetchContent)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    modules/sun.cc
    modules/wspr_tracker.cc
    modules/lunar.cc
    modules/pskreporter.cc
)

if(MSVC)
add_compile_options (-DCMAKE_BUILD_PARALLEL_LEVEL=16)
 #   add_compile_options(/W4 /WX)
#	add_compile_options (-DCMAKE_BUILD_PARALLEL_LEVEL=16 -DCLOCK_DEBUG)
#	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
#	set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} /DEBUG")
#	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /RTC1 /MDd")
#	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob0")
else()
#    add_compile_options(-Wall -Wextra -Werror -Os)
     add_compile_options (-pg)
endif()

function (find_build symbol path tag target)
if (NOT WIN32)
	find_package(${symbol} QUIET)
endif()
 	if (NOT ${symbol}_FOUND)
		message(STATUS "${symbol} not found, fetching...")
		FetchContent_Declare(
			${symbol}
			GIT_REPOSITORY ${path}
			PREFIX ${EXTERNAL_DIR}/${symbol}
 			GIT_TAG ${tag}
		)
		FetchContent_MakeAvailable(${symbol})
		 set(${symbol}_SOURCE_DIR ${${symbol}_SOURCE_DIR} PARENT_SCOPE)
        set(${symbol}_BINARY_DIR ${${symbol}_BINARY_DIR} PARENT_SCOPE)
		target_include_directories(clock PRIVATE
		${EXTERNAL_DIR}/${target} ${${symbol}_SOURCE_DIR}/include)
	else()
		message(STATUS "Using System ${symbol} ")
	endif()
endfunction()

add_executable(clock ${SOURCES})

if(WIN32)
	set(CMAKE_CXX_STANDARD 17)
	 execute_process(
    COMMAND git config --global --add safe.directory "*"
    RESULT_VARIABLE result
)
    target_compile_definitions(clock PRIVATE _CRT_SECURE_NO_WARNINGS)
    set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Windows")
    list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_DIR}")
#    target_include_directories(clock PRIVATE ${EXTERNAL_DIR}/json-develop/include)
	find_build(SDL3 "https://github.com/libsdl-org/SDL.git" release-3.2.x .)
	find_build(SDL3_ttf "https://github.com/libsdl-org/SDL_ttf.git" release-3.2.2 .)
	find_build(SDL3_image "https://github.com/libsdl-org/SDL_image.git" release-3.2.4
	sdl3_image-src/include)
    find_build("paho-mqtt3c" "https://github.com/eclipse-paho/paho.mqtt.c.git" "master" .)
	find_build(nlohmann_json "https://github.com/nlohmann/json.git" "master" .)
	find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" .)
#    find_package(SDL3_image REQUIRED)
    set(SGP4_LIB sgp4)
	message(STATUS "SGP4_SOURCE_DIR = ${sgp4_SOURCE_DIR}")
	message(STATUS "SGP4_BINARY_DIR = ${sgp4_BINARY_DIR}")
	target_include_directories(clock PRIVATE ${sgp4_SOURCE_DIR})
    target_link_libraries(clock PRIVATE winhttp sgp4 SDL3::SDL3
    SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf ${SGP4_LIB} paho-mqtt3c)
	


elseif(APPLE)
	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/macos")
	set(FETCHCONTENT_BASE_DIR ${EXTERNAL_DIR})
	find_package(ZLIB REQUIRED)
	find_build(CURL "https://github.com/curl/curl.git" master .)  	

	target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl m) 

	find_build(SDL3 "https://github.com/libsdl-org/SDL.git" release-3.2.18 .)
	find_build(SDL3_ttf "https://github.com/libsdl-org/SDL_ttf.git" release-3.2.2 .)
	find_build(SDL3_image "https://github.com/libsdl-org/SDL_image.git" release-3.2.4
	sdl3_image-src/include)
	find_build("paho-mqtt3c" "https://github.com/eclipse-paho/paho.mqtt.c.git" "master" .)
	find_build(nlohmann_json "https://github.com/nlohmann/json.git" "master" .)
	find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" "sgp4-src/")
	set(SGP4_LIB sgp4)
	target_link_libraries(clock PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf 
	sgp4 nlohmann_json::nlohmann_json SDL3_image::SDL3_image paho-mqtt3c)

else()
	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/linux")
	set(FETCHCONTENT_BASE_DIR ${EXTERNAL_DIR})
	find_package(ZLIB REQUIRED)
	find_build(CURL "https://github.com/curl/curl.git" master .)
	find_package(Fontconfig REQUIRED)
	target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl m fontconfig) 

	find_build(SDL3 "https://github.com/libsdl-org/SDL.git" release-3.2.18 .)
	find_build(SDL3_ttf "https://github.com/libsdl-org/SDL_ttf.git" release-3.2.2 .)
        find_build(SDL3_image "https://github.com/libsdl-org/SDL_image.git" release-3.2.4 .)
	find_build("paho-mqtt3c" "https://github.com/eclipse-paho/paho.mqtt.c.git" "master" .)
	find_build(nlohmann_json "https://github.com/nlohmann/json.git" "master" .)
	find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" "sgp4-src/")
	set(SGP4_LIB sgp4)
	target_link_libraries(clock PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf 
	sgp4 nlohmann_json::nlohmann_json SDL3_image::SDL3_image paho-mqtt3c)
endif()




