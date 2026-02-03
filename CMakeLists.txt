# ================================================================================
# CMAKE ENVIRONMENT SETUP
# ================================================================================

cmake_minimum_required(VERSION 3.16...4.0)
project(AaediClock)
include(FetchContent)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ================================================================================
# PROJECT SETUP
# ================================================================================

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
    modules/contests.cc
    modules/rss.cc
    modules/aurora.cc
)

add_executable(clock ${SOURCES})

# ================================================================================
# CUSTOM DEPENDANCY FUNCTION
# ================================================================================

function (find_build symbol path tag target)
if (NOT WIN32)
	find_package(${symbol} QUIET)
endif()
 	if (NOT ${symbol}_FOUND)
		message(STATUS "${symbol} not found, fetching...")
		FetchContent_Declare(
			${symbol}
			GIT_REPOSITORY ${path}
			GIT_PROGRESS TRUE
			PREFIX ${EXTERNAL_DIR}/${symbol}
 			GIT_TAG ${tag}
		)
		FetchContent_MakeAvailable(${symbol})
		 set(${symbol}_SOURCE_DIR ${${symbol}_SOURCE_DIR} PARENT_SCOPE)
        set(${symbol}_BINARY_DIR ${${symbol}_BINARY_DIR} PARENT_SCOPE)
		target_include_directories(clock SYSTEM PRIVATE
		${EXTERNAL_DIR}/${target} ${${symbol}_SOURCE_DIR}/include)
	else()
		message(STATUS "Using System ${symbol} ")
	endif()
endfunction()


# ================================================================================
# PLATFORM SPECIFIC COMPILER FLAGS
# ================================================================================
# cmake -DCMAKE_BUILD_TYPE=Release .. 	== release build
# cmake -DCMAKE_BUILD_TYPE=Debug ..	== Debug Build
# cmake ..				== last Cmake in that dir, Release by default
#
# Ensure build type is always set
if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(WIN32)
 	target_compile_options(clock PRIVATE
        	$<$<CONFIG:Debug>:/Od /Zi /W4 /WX /RTC1 /MDd /Ob0 /external:W0  /external:anglebrackets>
        	$<$<CONFIG:Release>:/O2 /DNDEBUG>
    	)	
 	target_compile_definitions(clock PRIVATE 
        	$<$<CONFIG:Debug>:CLOCK_DEBUG>
    	)
	target_link_options(clock PRIVATE
    		$<$<CONFIG:Debug>:/DEBUG>
	)

elseif(APPLE)
   	target_compile_options(clock PRIVATE
        	$<$<CONFIG:Debug>:-Wall -Wextra -O0 -pg -g>
        	$<$<CONFIG:Release>:-O3 -DNDEBUG>
    	)
 	target_compile_definitions(clock PRIVATE 
        	$<$<CONFIG:Debug>:CLOCK_DEBUG>
    	)
else()
   	target_compile_options(clock PRIVATE
        	$<$<CONFIG:Debug>:-Wall -Wextra -Werror -Wunused -O0 -pg -g>
        	$<$<CONFIG:Release>:-O3 -DNDEBUG>
    	)
	target_compile_definitions(clock PRIVATE 
        	$<$<CONFIG:Debug>:CLOCK_DEBUG>
    	)
endif()



# ================================================================================
# PLATFORM SPECIFIC SETUP
# ================================================================================
if(WIN32)
	set(CMAKE_CXX_STANDARD 11)
	execute_process(
    			COMMAND git config --global --add safe.directory "*"
    			RESULT_VARIABLE result
	)
    	target_compile_definitions(clock PRIVATE _CRT_SECURE_NO_WARNINGS)
    	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Windows")
    	list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_DIR}")
#    	target_include_directories(clock PRIVATE ${EXTERNAL_DIR}/json-develop/include)
	find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" .)
	
elseif(APPLE)
	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/macos")
	set(FETCHCONTENT_BASE_DIR ${EXTERNAL_DIR})

	find_package(ZLIB REQUIRED)
	find_build(CURL "https://github.com/curl/curl.git" master .)  	
	target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl m) 
        find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" "sgp4-src/")
else()
	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/linux")
	set(FETCHCONTENT_BASE_DIR ${EXTERNAL_DIR})
	find_package(ZLIB REQUIRED)
	find_build(CURL "https://github.com/curl/curl.git" master .)
	find_package(Fontconfig REQUIRED)
	target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl m fontconfig) 
        find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" "sgp4-src/")

endif()

# ================================================================================
# PLATFORM INDEPENDANT FETCHES
# ================================================================================
	set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
	find_build(SDL3 "https://github.com/libsdl-org/SDL.git" release-3.2.x .)
	find_build(SDL3_ttf "https://github.com/libsdl-org/SDL_ttf.git" release-3.2.2 .)
	find_build(SDL3_image "https://github.com/libsdl-org/SDL_image.git" release-3.2.4 sdl3_image-src/include)
	set(MAHO_MQTTC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(MAHO_MQTTC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
	find_build("paho-mqtt3c" "https://github.com/eclipse-paho/paho.mqtt.c.git" "master" .)
	find_build(nlohmann_json "https://github.com/nlohmann/json.git" "master" .)
	set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)
	find_build(libxml2 "https://gitlab.gnome.org/GNOME/libxml2.git" "master" .)
	
	set(SGP4_LIB sgp4)

# ================================================================================
# PLATFORM DEPENDANT FETCHES AND CLEANUP
# ================================================================================

if(WIN32)
        target_include_directories(clock
            SYSTEM PRIVATE
            ${CMAKE_BINARY_DIR}/_deps/paho-mqtt3c-src/src
        )
    	target_include_directories(clock PRIVATE ${sgp4_SOURCE_DIR})
    	target_link_libraries(clock PRIVATE winhttp sgp4 SDL3::SDL3
    	SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf ${SGP4_LIB} paho-mqtt3c LibXml2::LibXml2)

elseif(APPLE)
    	target_link_libraries(clock PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf 
	sgp4 nlohmann_json::nlohmann_json SDL3_image::SDL3_image paho-mqtt3c LibXml2::LibXml2)
else()
	target_link_libraries(clock PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf 
	sgp4 nlohmann_json::nlohmann_json SDL3_image::SDL3_image paho-mqtt3c LibXml2::LibXml2)
endif()

