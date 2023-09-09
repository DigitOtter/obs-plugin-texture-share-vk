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

find_package(TextureShareVulkan REQUIRED)

if(NOT DEFINED BUILD_IN_OBS)
	set(BUILD_IN_OBS OFF)
endif()

if(NOT ${BUILD_IN_OBS})
	set(CMAKE_CXX_STANDARD 20)
	set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
## Library
set(LIB_SRC_FILES
	"obs_plugin_texture_share/obs_plugin_texture_share.cpp"
)

add_library("${LIBRARY_NAME}" SHARED ${LIB_SRC_FILES})
add_library("${NAMESPACE_NAME}::${LIBRARY_NAME}" ALIAS ${LIBRARY_NAME})
target_compile_options(${LIBRARY_NAME} PUBLIC $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:-Wall -Wextra>)

target_include_directories(${LIBRARY_NAME}
	PUBLIC 
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"

	PRIVATE
)

target_link_libraries(${LIBRARY_NAME}
	PUBLIC
		OBS::libobs
		TextureShareVulkan::TextureShareVulkan
	PRIVATE
)


##########################################
## Install files
if(${BUILD_IN_OBS})
		install_obs_plugin_with_data(${LIBRARY_NAME} data)
else()
		install(TARGETS ${LIBRARY_NAME}
				EXPORT ${LIB_EXPORT_NAME}
				LIBRARY DESTINATION "${OBS_PLUGIN_LIB_DIR}"
				ARCHIVE DESTINATION "${OBS_PLUGIN_LIB_DIR}")

		install(DIRECTORY "data/locale"
				DESTINATION "${OBS_PLUGIN_DATA_DIR}/${PROJECT_NAME}")
endif()

