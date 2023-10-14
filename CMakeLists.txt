set(PROJECT_NAME "OBSPluginTextureShare")
set(HEADER_DIRECTORY "obs_plugin_texture_share")

set(NAMESPACE_NAME "${PROJECT_NAME}")

set(LIBRARY_NAME "${PROJECT_NAME}")
set(EXECUTABLE_NAME "${PROJECT_NAME}Exec")
set(TEST_NAME "${PROJECT_NAME}Tests")

set(LIB_EXPORT_NAME "${LIBRARY_NAME}Targets")
set(LIB_CONFIG_NAME "${LIBRARY_NAME}Config")
set(LIB_VERSION_NAME "${LIB_CONFIG_NAME}Version")

cmake_minimum_required(VERSION 3.20)
project("${PROJECT_NAME}" VERSION 1.0.0)

include(GNUInstallDirs)

find_package(TextureShareVk REQUIRED)
find_package(GLUT REQUIRED)
find_package(GLEW REQUIRED)

if(NOT DEFINED BUILD_IN_OBS)
	set(BUILD_IN_OBS OFF)
endif()

if(NOT ${BUILD_IN_OBS})
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

	find_package(libobs REQUIRED)
	find_package(obs-frontend-api REQUIRED)

	if(NOT DEFINED LIBOBS_PLUGIN_DESTINATION)
		set(LIBOBS_PLUGIN_DESTINATION "lib/obs-plugins")
	endif()

	if(NOT DEFINED LIBOBS_PLUGIN_DATA_DESTINATION)
		set(LIBOBS_PLUGIN_DATA_DESTINATION "share/obs/obs-plugins")
	endif()

	set(OBS_PLUGIN_LIB_DIR "${LIBOBS_PLUGIN_DESTINATION}" CACHE PATH "Path to obs plugins (relative to CMAKE_INSTALL_PREFIX)")
	set(OBS_PLUGIN_DATA_DIR "${LIBOBS_PLUGIN_DATA_DESTINATION}" CACHE PATH "Path to scene tree view data dir (relative to CMAKE_INSTALL_PREFIX)")

	if(WIN32)
		message(WARNING "Building ${PROJECT_NAME} outside of OBS is currently not supported")
	endif()
endif()

##########################################
## Plugin Libraries
function(tsp_plugin_setup target)
	add_library("${NAMESPACE_NAME}::${target}" ALIAS ${target})
	target_compile_options(${target} PUBLIC $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:-Wall -Wextra>)

	target_include_directories(${target}
		PUBLIC
		    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
			"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
		PRIVATE)

	target_link_libraries(${target}
		PUBLIC
		    OBS::libobs
			TextureShareVk::TextureShareGlClientCpp
		PRIVATE)
endfunction()

add_library(TextureShareFilter SHARED "obs_plugin_texture_share/obs_plugin_texture_share_filter.cpp")
tsp_plugin_setup(TextureShareFilter)

add_library(TextureShareSource SHARED "obs_plugin_texture_share/obs_plugin_texture_share_source.cpp")
tsp_plugin_setup(TextureShareSource)


##########################################
## Install files
if(${BUILD_IN_OBS})
	    install_obs_plugin_with_data(TextureShareFilter data)
		install_obs_plugin_with_data(TextureShareSource data)
else()
	    install(TARGETS TextureShareFilter TextureShareSource
			    EXPORT ${LIB_EXPORT_NAME}
				LIBRARY DESTINATION "${OBS_PLUGIN_LIB_DIR}"
				ARCHIVE DESTINATION "${OBS_PLUGIN_LIB_DIR}")

		install(DIRECTORY "data/locale"
			    DESTINATION "${OBS_PLUGIN_DATA_DIR}/${PROJECT_NAME}")
endif()

