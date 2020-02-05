cmake_minimum_required(VERSION 2.8.12)

project(OSVRInstaller)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

############################################################
# Required packages
############################################################
find_package(Git REQUIRED)

############################################################
# Configuration Step
############################################################
# Build options
option( BUILD_EXAMPLES "Make API examples" OFF )
option( BUILD_TESTS "Build test functions" OFF )
option( USE_SUPERBUILD "Build all dependencies in SUPERBUILD mode" ON )

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/cmake
    ${CMAKE_INSTALL_PREFIX}/cmake
)

list(APPEND CMAKE_PREFIX_PATH
    ${CMAKE_INSTALL_PREFIX}/lib/cmake
    usr/lib/cmake
)

# The superbuild file will recursively call this CMakeLists.txt
# with USE_SUPERBUILD set to off to skip the SUPERBUILD part
if(USE_SUPERBUILD)
   include(ExternalProject)
   include(Superbuild.cmake)
   return()
endif(USE_SUPERBUILD)

############################################################
# Build Step
############################################################

#install(
#  FILES
#    # ${DAEMON_HEADERS}
#  DESTINATION
#    include
#  COMPONENT
#    headers
#)
