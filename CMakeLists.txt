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
option( BUILD_EXAMPLES "Make API examples" ON )
option( BUILD_TESTS "Build test functions" ON )
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

# Install the sample server configuration files and scripts.

install(
  FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/vendored/OSVR-RenderManager/installer/nondirectmode_window.json
    ${CMAKE_CURRENT_SOURCE_DIR}/nondirectmode_window.bat
    ${CMAKE_CURRENT_SOURCE_DIR}/nondirectmode_window.sh
  PERMISSIONS
    OWNER_EXECUTE OWNER_WRITE OWNER_READ
    GROUP_EXECUTE GROUP_READ
    WORLD_EXECUTE WORLD_READ
  DESTINATION bin
)
