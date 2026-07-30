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
if(NOT DEFINED ASAN_ENABLE)
	set(ASAN_ENABLE OFF CACHE BOOL "Enable Address Sanitizer" FORCE)
endif()
if (ASAN_ENABLE)
	message(STATUS "Address Sanitizer set to ... ENABLED")
else()
	message(STATUS "Address Sanitizer set to ... DISABLED")
endif()
set(CURL_NAME )
if (NOT WIN32)
	set(CURL_NAME CURL::libcurl)
endif()
set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

set (SOURCES
	core/init.cc
	core/classes.cc
	core/panels.cc
	core/itterate.cc
	core/quit.cc
	core/event.cc
	utils/http_fetch.cc
	utils/conversions.cc
	plugins/host_api.cc
)

add_executable(clock ${SOURCES})

# set up for stuff common to all libs and sub modules

add_library(aaediclock_path INTERFACE)
target_include_directories(aaediclock_path INTERFACE ${CMAKE_SOURCE_DIR})
if (ASAN_ENABLE) 
	target_compile_options(aaediclock_path INTERFACE -fsanitize=address)
	target_link_options(aaediclock_path INTERFACE -fsanitize=address)
endif()


# ================================================================================
# PLATFORM SPECIFIC COMPILER FLAGS
# ================================================================================
# cmake -DFREETYPE_LIBRARY=/opt/homebrew/lib/libfreetype.dylib -DFREETYPE_INCLUDE_DIRS=/opt/homebrew/include/freetype2 ..
#					== MacOS FreeType Pathing
# cmake -DCMAKE_BUILD_TYPE=Release .. 	== Release build
# cmake -DCMAKE_BUILD_TYPE=Debug ..	== Debug Build
# cmake -DASAN_ENABLE=ON ..		== Enable Address Sanitizer on Posix
# cmake -DSDL_UNIX_CONSOLE_BUILD=ON ..	== Disable SDL3 X11/Wayland Back End
# cmake ..				== last Cmake in that dir, Release by default
#

# Ensure build type is always set
if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(WIN32)
	target_link_libraries(aaediclock_path INTERFACE  SDL3::SDL3 winhttp)
	target_compile_options(clock PRIVATE /external:W0  /external:anglebrackets /showIncludes
        	$<$<CONFIG:Debug>:/Od /Zi /W4 /WX /RTC1 /MDd /Ob0 >
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
	target_compile_options(aaediclock_path INTERFACE 
		-fno-omit-frame-pointer -rdynamic
	)
else()
	if (ASAN_ENABLE)
        	target_compile_options(clock PRIVATE
                	$<$<CONFIG:Debug>:-Wall -Wextra -Werror -Wunused -O0 -fsanitize=address -pg -g >
	                $<$<CONFIG:Release>:-O3 -DNDEBUG>
        	)
		target_link_options(clock PRIVATE
    			$<$<CONFIG:Debug>:LINKER:--no-undefined,-E,-fsanitize=address>
			$<$<CONFIG:Release>:LINKER: --export-dynamic>
		)
	else ()
        	target_compile_options(clock PRIVATE
                	$<$<CONFIG:Debug>:-Wall -Wextra -Werror -Wunused -O0 -pg -g >
	                $<$<CONFIG:Release>: -O3>
        	)
		target_link_options(clock PRIVATE
    			$<$<CONFIG:Debug>:LINKER:--no-undefined,-E>
			$<$<CONFIG:Release>:LINKER: --export-dynamic>
		)
	endif()
	target_compile_definitions(clock PRIVATE 
        	$<$<CONFIG:Debug>:CLOCK_DEBUG>
    	)
	target_compile_options(aaediclock_path INTERFACE 
		-fno-omit-frame-pointer
	)
endif()


# ================================================================================
# Main Project starts here
# ================================================================================




target_link_libraries(clock PRIVATE aaediclock_path LibXml2::LibXml2)

#---------------------------
# libraries start here
#---------------------------
add_library(ncdxf_plugin SHARED
    plugins/ncdxf.cc
)
target_link_libraries(ncdxf_plugin PRIVATE aaediclock_path)

add_library(callsign_plugin SHARED
    plugins/callsign.cc
)
target_link_libraries(callsign_plugin PRIVATE aaediclock_path)

add_library(clock_plugin SHARED
    plugins/clock.cc
)
target_link_libraries(clock_plugin PRIVATE aaediclock_path)

add_library(pota_plugin SHARED
    plugins/pota.cc
    utils/http_fetch.cc
)
target_link_libraries(pota_plugin PRIVATE aaediclock_path SDL3::SDL3 nlohmann_json::nlohmann_json ${CURL_NAME})

add_library(de_plugin SHARED
    plugins/de.cc
    utils/maidenhead.cc
    utils/celestials.cc
)
target_link_libraries(de_plugin PRIVATE aaediclock_path)

add_library(dx_plugin SHARED
    plugins/dx.cc
    utils/maidenhead.cc
    utils/celestials.cc
)
target_link_libraries(dx_plugin PRIVATE aaediclock_path)

add_library(dx_cluster_plugin SHARED
    plugins/dx_cluster.cc
    utils/http_fetch.cc
    utils/socket.cc
    utils/conversions.cc
)
target_link_libraries(dx_cluster_plugin PRIVATE aaediclock_path SDL3::SDL3 LibXml2::LibXml2 ${CURL_NAME})

#add_library(sat_tracker_plugin SHARED
#    plugins/sat_tracker.cc
#    utils/http_fetch.cc
#    utils/conversions.cc
#)
#target_link_libraries(sat_tracker_plugin PRIVATE aaediclock_path sgp4 SDL3::SDL3 SDL3_image::SDL3_image ${CURL_NAME})
add_library(sat_tracker_plugin SHARED
    plugins/sat_tracker.cc 
    plugins/SGP4/SGP4.cpp
    utils/http_fetch.cc
    utils/conversions.cc
)
target_link_libraries(sat_tracker_plugin PRIVATE aaediclock_path SDL3::SDL3 SDL3_image::SDL3_image LibXml2::LibXml2 ${CURL_NAME})
#add_library(new_sat_tracker_plugin SHARED
#    plugins/new_sat_tracker.cc 
#    plugins/SGP4/SGP4.cpp
#    utils/http_fetch.cc
#    utils/conversions.cc
#)
#temporary for Vallado test
#target_compile_options(new_sat_tracker_plugin PRIVATE -fpermissive)
#target_link_libraries(new_sat_tracker_plugin PRIVATE aaediclock_path SDL3::SDL3 SDL3_image::SDL3_image LibXml2::LibXml2 ${CURL_NAME})


add_library(aurora_plugin SHARED
    plugins/aurora.cc
    utils/http_fetch.cc
)
target_link_libraries(aurora_plugin PRIVATE aaediclock_path SDL3::SDL3 nlohmann_json::nlohmann_json ${CURL_NAME})

add_library(sdo_plugin SHARED
    plugins/sun.cc
    utils/http_fetch.cc
    utils/celestials.cc
)
target_link_libraries(sdo_plugin PRIVATE aaediclock_path SDL3::SDL3 SDL3_image::SDL3_image ${CURL_NAME})

add_library(contest_plugin SHARED
    plugins/contests.cc
    utils/http_fetch.cc
)
target_link_libraries(contest_plugin PRIVATE aaediclock_path SDL3::SDL3 LibXml2::LibXml2 ${CURL_NAME})

add_library(kindex_plugin SHARED
    plugins/kindex.cc
    utils/http_fetch.cc
)
target_link_libraries(kindex_plugin PRIVATE aaediclock_path SDL3::SDL3 nlohmann_json::nlohmann_json ${CURL_NAME})


add_library(wspr_plugin SHARED
    plugins/wspr_tracker.cc
    utils/http_fetch.cc
    utils/conversions.cc
)
target_link_libraries(wspr_plugin PRIVATE aaediclock_path SDL3::SDL3 ${CURL_NAME})


add_library(lunar_plugin SHARED
    plugins/lunar.cc
    utils/celestials.cc
)
target_link_libraries(lunar_plugin PRIVATE aaediclock_path SDL3::SDL3 SDL3_image::SDL3_image)


add_library(rss_plugin SHARED
    plugins/rss.cc
    utils/http_fetch.cc
)
target_link_libraries(rss_plugin PRIVATE aaediclock_path LibXml2::LibXml2 ${CURL_NAME})

add_library(pskreporter_plugin SHARED
    plugins/pskreporter.cc
    utils/http_fetch.cc
    utils/maidenhead.cc
)
target_link_libraries(pskreporter_plugin PRIVATE aaediclock_path paho-mqtt3c LibXml2::LibXml2 ${CURL_NAME})

add_library(muf_plugin SHARED
    plugins/muf.cc
    utils/http_fetch.cc
)
target_link_libraries(muf_plugin PRIVATE aaediclock_path LibXml2::LibXml2 ${CURL_NAME})


add_library(map_plugin SHARED
	plugins/map.cc
	utils/celestials.cc
)
target_link_libraries(map_plugin PRIVATE aaediclock_path  SDL3::SDL3 SDL3_image::SDL3_image)

#==============================
#libraries end here
#==============================

set(PLUGIN_TARGETS
    ncdxf_plugin 
    callsign_plugin
    clock_plugin
    pota_plugin
    de_plugin
    dx_plugin
    dx_cluster_plugin
    sat_tracker_plugin
#    new_sat_tracker_plugin
    aurora_plugin
    sdo_plugin
    contest_plugin
    kindex_plugin
    wspr_plugin
    lunar_plugin
    map_plugin
    pskreporter_plugin
    rss_plugin
    muf_plugin
)

# ================================================================================
# CUSTOM DEPENDANCY FUNCTION
# ================================================================================

function (find_build symbol path tag target)
	if (NOT WIN32)
		find_package(${symbol} QUIET)
	endif()
	if (NOT ${symbol}_FOUND)
		message(STATUS "${symbol} not found, fetching...")
		find_program(GIT_EXECUTABLE git)
                if(NOT GIT_EXECUTABLE)
    			message(FATAL_ERROR
        			"git was not found, but is required to fetch third-party dependencies.\n"
        			"Install git or manually pre-install dependancies.")
		endif()
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
		target_include_directories(aaediclock_path INTERFACE
			${EXTERNAL_DIR}/${target} ${${symbol}_SOURCE_DIR}/include)
	else()
		message(STATUS "Using System ${symbol} ")
	endif()
endfunction()


# ================================================================================
# PLATFORM SPECIFIC SETUP
# ================================================================================
if(WIN32)
	execute_process(
    			COMMAND git config --global --add safe.directory "*"
    			RESULT_VARIABLE result
	)
        set(LIBXML2_WITH_ICONV OFF CACHE BOOL "" FORCE)
    	target_compile_definitions(clock PRIVATE _CRT_SECURE_NO_WARNINGS)
    	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Windows")
    	list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_DIR}")

    	target_include_directories(clock PRIVATE ${EXTERNAL_DIR}/json-develop/include)
#	find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master"  "sgp4-src/")
#        target_include_directories(sat_tracker_plugin SYSTEM PRIVATE ${sgp4_SOURCE_DIR})
	find_build(libxml2 "https://gitlab.gnome.org/GNOME/libxml2.git" "master" .)
	
elseif(APPLE)
	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/macos")
	set(FETCHCONTENT_BASE_DIR ${EXTERNAL_DIR})

	find_package(ZLIB REQUIRED)
	find_build(CURL "https://github.com/curl/curl.git" master .)  	
	target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl m) 
 #       find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" "sgp4-src/")
 #       target_include_directories(sat_tracker_plugin SYSTEM PRIVATE ${sgp4_SOURCE_DIR})
	find_build(libXml2 "https://gitlab.gnome.org/GNOME/libxml2.git" "master" .)
	list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
	find_build(Fontconfig "https://gitlab.freedesktop.org/fontconfig/fontconfig.git" "main" .)
	
else()
	set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/linux")
	set(FETCHCONTENT_BASE_DIR ${EXTERNAL_DIR})

	find_package(ZLIB REQUIRED)
	find_build(CURL "https://github.com/curl/curl.git" master .)
        find_build(Fontconfig "https://gitlab.freedesktop.org/fontconfig/fontconfig.git" "main" .)
#	find_package(Fontconfig REQUIRED)
	target_link_libraries(clock PRIVATE ZLIB::ZLIB CURL::libcurl m fontconfig) 
 #       find_build(sgp4 https://github.com/dnwrnr/sgp4.git "master" "sgp4-src/")
 #       target_include_directories(sat_tracker_plugin SYSTEM PRIVATE ${sgp4_SOURCE_DIR})
	find_build(libxml2 "https://gitlab.gnome.org/GNOME/libxml2.git" "master" .)

endif()

# ================================================================================
# PLATFORM INDEPENDANT FETCHES
# ================================================================================
	set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
	find_build(SDL3 "https://github.com/libsdl-org/SDL.git" release-3.2.x .)
	find_build(SDL3_ttf "https://github.com/libsdl-org/SDL_ttf.git" release-3.2.2 .)
	set(SDL3IMAGE_AVIF OFF)
	find_build(SDL3_image "https://github.com/libsdl-org/SDL_image.git" release-3.2.4 sdl3_image-src/include)
	set(MAHO_MQTTC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(MAHO_MQTTC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
	find_build("paho-mqtt3c" "https://github.com/eclipse-paho/paho.mqtt.c.git" "master" .)
	find_build(nlohmann_json "https://github.com/nlohmann/json.git" "master" .)
	set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)
#	set(SGP4_LIB sgp4)

# ================================================================================
# PLATFORM DEPENDANT FETCHES AND CLEANUP
# ================================================================================

if(WIN32)
	target_include_directories(clock SYSTEM PRIVATE
		${CMAKE_BINARY_DIR}/_deps/paho-mqtt3c-src/src
        )
    	target_include_directories(clock PRIVATE)
    	target_link_libraries(clock PRIVATE 
		winhttp SDL3::SDL3
	    	SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf LibXml2::LibXml2)

	# copy target binaries to binary directory in build tree
	#---------------------------------------------------------
        set(BINARY_TREE ${CMAKE_BINARY_DIR}/binary)
        add_custom_command(TARGET clock POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_TREE}")
	# copy binary
        add_custom_command(TARGET clock POST_BUILD 
        	COMMAND ${CMAKE_COMMAND} -E copy_if_different
            	$<TARGET_FILE:clock>
            	${BINARY_TREE}
        )
	# copy pdb
        add_custom_command(TARGET clock POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_PDB_FILE:clock>
            ${BINARY_TREE}
        )
	#copy SDL3
        add_custom_command(TARGET clock POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            ${BINARY_TREE}
        )
	#copy SDL3 Image
        add_custom_command(TARGET clock POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3_image::SDL3_image>
            ${BINARY_TREE}
        )
	# copy SDL3 TTF
        add_custom_command(TARGET clock POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
            ${BINARY_TREE}
        )
	# copy LibXML
        add_custom_command(TARGET clock POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:LibXml2::LibXml2>
            ${BINARY_TREE}
        )
	# copy MQTT Lib
        add_custom_command(TARGET clock POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:paho-mqtt3c>
            ${BINARY_TREE}
        )
	# copy plugins
        foreach(plugin IN LISTS PLUGIN_TARGETS)
            add_custom_command(TARGET ${plugin} POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_TREE}")
            add_custom_command(TARGET ${plugin} POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_TREE}/plugins/" )
            add_custom_command(TARGET ${plugin} POST_BUILD 
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${plugin}>
                ${BINARY_TREE}/plugins/
            )
            add_custom_command(TARGET ${plugin} POST_BUILD 
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_PDB_FILE:${plugin}>
                ${BINARY_TREE}/plugins/
            )
        endforeach()

elseif(APPLE)
	target_link_libraries(clock PRIVATE
        	SDL3::SDL3 SDL3_ttf::SDL3_ttf nlohmann_json::nlohmann_json
        	SDL3_image::SDL3_image LibXml2::LibXml2 Fontconfig::Fontconfig)

else()
	target_link_libraries(clock PRIVATE 
		SDL3::SDL3 SDL3_ttf::SDL3_ttf nlohmann_json::nlohmann_json SDL3_image::SDL3_image LibXml2::LibXml2)
endif()
