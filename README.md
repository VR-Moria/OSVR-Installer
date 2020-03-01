# OSVR-Installer

This project provides a CMakeLists.txt and Superbuild.cmake file to build
the dependencies of the OSVR-RenderManager project (other than the
prerequisites described below) assuming that the system has a compiler
and CMake installed.

It places all executables in the INSTALL/bin directory under the
build directory (including the samples and test programs if they are
configured in the build), the headers in INSTALL/include and the libraries in
INSTALL/lib.

## Running on Windows

The file *nondirectmode_window.bat* will run an OSVR server with a configuration
file that shows a window on the main display and the file
*nondirectmode_window_lookabout.bat* will show it with stored user motion.
These scripts must be run from within
the INSTALL/bin directory where they live.  Once one of them is running, any of the
RenderManager demo programs can be run; they must be run from within the
INSTALL/bin directory for the lookabout approach because that's where the stored
data file is.

This project also comes with an osvrInstaller.aip file, which is and Advanced
Installer script to generate an MSI file for installation on Windows.  It
assumes that the CMake build directory has been set to C:\tmp\vs2019\OSVR-Installer
so that it can find the installed files where it expects them.

Once installed, the *osvr_server nondirectmode_window* shortcut can be used to
start the server and then any of the RenderManager demo shortcuts can be used
to display.

## Running on Linux and Mac

The file *nondirectmode_window.sh* will run an OSVR server with a configuration
file that shows a window on the main display and the file
*nondirectmode_window_lookabout.sh* will show it with stored user motion.
These scripts must be run from within
the INSTALL/bin directory where they live.  Once one of them is running, any of the
RenderManager demo programs can be run; they must be run from within the
INSTALL/bin directory for the lookabout approach because that's where the stored
data file is.

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
