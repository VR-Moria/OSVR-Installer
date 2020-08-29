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

# Install text-based sample application that shows how to render
# textured objects in OSVR using OpenGL.

if (APPLE)
  add_definitions(-DGL_SILENCE_DEPRECATION)
endif (APPLE)

find_package(osvrRenderManager CONFIG REQUIRED)
osvrrm_find_SDL2()
osvrrm_find_GLEW()
find_package(OpenGL REQUIRED)
find_package(Freetype CONFIG REQUIRED)
add_executable(OpenGLCoreTextureExample OpenGLCoreTextureExample.cpp)
#target_include_directories(OpenGLCoreTextureExample PRIVATE
#  ${CMAKE_INSTALL_INCUDEDIR}
#)
target_link_libraries(OpenGLCoreTextureExample PRIVATE
  osvrRenderManager::osvrRenderManagerCpp
  freetype
  GLEW::GLEW
  SDL2::SDL2
  ${OPENGL_LIBRARY}
)
target_compile_features(OpenGLCoreTextureExample PRIVATE cxx_range_for)

message(info "XXX CMAKE_INSTALL_BINDER = ${CMAKE_INSTALL_BINDIR}")
install(TARGETS OpenGLCoreTextureExample
  DESTINATION bin)

# Install the sample server configuration files and scripts.

install(
  FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/vendored/OSVR-RenderManager/installer/nondirectmode_window.json
    ${CMAKE_CURRENT_SOURCE_DIR}/nondirectmode_window.bat
    ${CMAKE_CURRENT_SOURCE_DIR}/nondirectmode_window.sh
    ${CMAKE_CURRENT_SOURCE_DIR}/lookabout.vrpn
    ${CMAKE_CURRENT_SOURCE_DIR}/nondirectmode_window_lookabout.json
    ${CMAKE_CURRENT_SOURCE_DIR}/nondirectmode_window_lookabout.bat
    ${CMAKE_CURRENT_SOURCE_DIR}/nondirectmode_window_lookabout.sh
    ${CMAKE_CURRENT_SOURCE_DIR}/COURIER.TTF
  PERMISSIONS
    OWNER_EXECUTE OWNER_WRITE OWNER_READ
    GROUP_EXECUTE GROUP_READ
    WORLD_EXECUTE WORLD_READ
  DESTINATION bin
)
