cmake_minimum_required(VERSION 3.5)

set(PRIMARY_PROJECT_NAME UKFTractography)
#-----------------------------------------------------------------------------
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})
include(ExternalProject)
include(ExternalProjectDependency)

#-----------------------------------------------------------------------------
if(APPLE)
  # Note: By setting CMAKE_OSX_* variables before any enable_language() or project() calls,
  #       we ensure that the bitness, and C++ standard library will be properly detected.
  include(SlicerBlockSetCMakeOSXVariables)
  mark_as_superbuild(
    VARS CMAKE_OSX_ARCHITECTURES:STRING CMAKE_OSX_SYSROOT:PATH CMAKE_OSX_DEPLOYMENT_TARGET:STRING
    ALL_PROJECTS
    )
endif()

#-----------------------------------------------------------------------------

option(${PRIMARY_PROJECT_NAME}_BUILD_SLICER_EXTENSION "Build ${PRIMARY_PROJECT_NAME} as Slicer extension." OFF)

if (DEFINED Slicer_DIR)
  set(${PRIMARY_PROJECT_NAME}_BUILD_SLICER_EXTENSION ON)
endif()

#-----------------------------------------------------------------------------
option(${PRIMARY_PROJECT_NAME}_SUPERBUILD "Build ${PRIMARY_PROJECT_NAME} and the projects it depends on via SuperBuild.cmake." ${default_superbuild_value})
mark_as_advanced(${PRIMARY_PROJECT_NAME}_SUPERBUILD)

#-----------------------------------------------------------------------------
if(${PRIMARY_PROJECT_NAME}_BUILD_SLICER_EXTENSION)
  include(SlicerExtension.cmake)
else()
  set(default_superbuild_value ON)
endif()

#-----------------------------------------------------------------------------
# Required by Slicer extension build system for reasons. See
#    https://github.com/Slicer/Slicer/commit/b160ec13f276a86306513954ef8b08a5332afc2e
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${CMAKE_BINARY_DIR};${EXTENSION_NAME};RuntimeLibraries;/")

#-----------------------------------------------------------------------------
# Superbuild script
#-----------------------------------------------------------------------------

if(${PRIMARY_PROJECT_NAME}_SUPERBUILD)
  project(SuperBuild_${PRIMARY_PROJECT_NAME})  # <- NOTE: Project name for pre-requisites is different form main project
  include("${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild.cmake")
  return()
else()
  # Building against Slicer
  project(${PRIMARY_PROJECT_NAME})             # <- NOTE: Here is the main project name setting
  include("${CMAKE_CURRENT_SOURCE_DIR}/${PRIMARY_PROJECT_NAME}.cmake")
  return()
endif()
message(FATAL_ERROR "You should never reach this point !")
