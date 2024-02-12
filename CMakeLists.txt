set(PROJECT_NAME "OBSPluginTextureShareVk")
set(HEADER_DIRECTORY "obs_plugin_texture_share_vk")

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

    set(OBS_PLUGIN_LIB_DIR
        "${LIBOBS_PLUGIN_DESTINATION}"
        CACHE PATH "Path to obs plugins (relative to CMAKE_INSTALL_PREFIX)")
    set(OBS_PLUGIN_DATA_DIR
        "${LIBOBS_PLUGIN_DATA_DESTINATION}"
        CACHE
            PATH
            "Path to scene tree view data dir (relative to CMAKE_INSTALL_PREFIX)"
    )

    if(WIN32)
        message(
            WARNING
                "Building ${PROJECT_NAME} outside of OBS is currently not supported"
        )
    endif()
endif()

# ##############################################################################
# Plugin Libraries
function(tsp_plugin_setup target)
    add_library("${NAMESPACE_NAME}::${target}" ALIAS ${target})
    target_compile_options(
        ${target}
        PUBLIC $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:-Wall
               -Wextra>)

    target_include_directories(
        ${target}
        PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
               "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        PRIVATE)

    target_link_libraries(
        ${target}
        PUBLIC OBS::libobs TextureShareVk::TextureShareGlClientCpp
        PRIVATE)
endfunction()

add_library(TsvSendFilter SHARED
            "obs_plugin_texture_share_vk/tsv_send_filter.cpp")
tsp_plugin_setup(TsvSendFilter)

add_library(TsvReceiveSource SHARED
            "obs_plugin_texture_share_vk/tsv_receive_source.cpp")
tsp_plugin_setup(TsvReceiveSource)

# ##############################################################################
# Install files
if(${BUILD_IN_OBS})
    install_obs_plugin_with_data(TsvSendFilter data)
    install_obs_plugin_with_data(TsvReceiveSource data)
else()
    install(
        TARGETS TsvSendFilter TsvReceiveSource
        EXPORT ${LIB_EXPORT_NAME}
        LIBRARY DESTINATION "${OBS_PLUGIN_LIB_DIR}"
        ARCHIVE DESTINATION "${OBS_PLUGIN_LIB_DIR}")

    install(DIRECTORY "data/locale"
            DESTINATION "${OBS_PLUGIN_DATA_DIR}/${PROJECT_NAME}")
endif()
