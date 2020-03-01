# OSVR-Installer

This project provides a CMakeLists.txt and Superbuild.cmake file to build
the dependencies of the OSVR-RenderManager project (other than the
prerequisites described below) assuming that the system has a compiler
and CMake installed.

It places all DLLs and executables in the INSTALL/bin directory under the
build directory (including the samples and test programs if they are
configured in the build), the headers in INSTALL/include and the libraries in
INSTALL/lib.

## Prerequisites

As of February 2020, the CMake compilation of Boost is still in development,
so boost cannot be brought in as a submodule and compiled as part of the build.
This means that boost (along with at least its filesystem and system libraries)
must be installed on the system before this project can be built.

## Tested systems

As of 2/29/2020, the package has been tested on the following configurations:
* Windows 10, Microsoft Visual Studio 2019.
* Ubuntu 18.04.4, GCC 7.4.0
* macOS 10.15.3, Xcode with Apple clang 11.0.0
