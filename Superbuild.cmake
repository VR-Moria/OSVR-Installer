
set(cmake_common_args
  -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=${CMAKE_VERBOSE_MAKEFILE}
  -DCMAKE_C_COMPILER:PATH=${CMAKE_C_COMPILER}
  -DCMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
  -DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
  -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
  -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/INSTALL
  -DCMAKE_MACOSX_RPATH:BOOL=TRUE
  -DCMAKE_INSTALL_RPATH:PATH=${CMAKE_BINARY_DIR}/INSTALL/lib${LIB_SUFFIX}
  -DCMAKE_INCLUDE_PATH:PATH=${CMAKE_BINARY_DIR}/INSTALL/include
  -DLIB_SUFFIX:STRING=${LIB_SUFFIX}
  -DBUILD_EXAMPLES:BOOL=${BUILD_EXAMPLES}
  -DBUILD_TESTS:BOOL=${BUILD_TESTS}
  -DUSE_SUPERBUILD:BOOL=OFF
)

add_custom_target(submodule_init
    COMMAND ${GIT_EXECUTABLE} submodule update --init --checkout ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Only built if dependency
macro(add_external_project MYNAME LOCATION MASTER DEPENDS ARGS INIT_RECURSIVE)
    if(${INIT_RECURSIVE})
        set(SUBMODULE_UPDATE_OPTIONS "--init" "--checkout" "--recursive")
    else()
        set(SUBMODULE_UPDATE_OPTIONS "--checkout")
    endif(${INIT_RECURSIVE})
    ExternalProject_Add( ${MYNAME}
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/${LOCATION}
        BUILD_ALWAYS 1
        DOWNLOAD_COMMAND ${GIT_EXECUTABLE} submodule update ${SUBMODULE_UPDATE_OPTIONS} ${CMAKE_SOURCE_DIR}/${LOCATION}
        DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}
        CMAKE_ARGS ${cmake_common_args} ${ARGS}
        INSTALL_DIR ${CMAKE_BINARY_DIR}/INSTALL
        DEPENDS ${DEPENDS} submodule_init
    )
endmacro(add_external_project)

set(LIBFUNCTIONALITY_ARGS
    -DBUILD_TESTING:BOOL=OFF
)
add_external_project(libfunctionality vendored/libfunctionality OFF "" "${LIBFUNCTIONALITY_ARGS}" ON)

set(OSVRCORE_ARGS
    -DBUILD_TESTING:BOOL=OFF
)
add_external_project(OSVR-Core vendored/OSVR-Core OFF "libfunctionality" "${OSVRCORE_ARGS}" ON)

add_external_project(jsoncpp vendored/jsoncpp OFF "" "" ON)

set(GLEW_ARGS
    -DONLY_LIBS:BOOL=ON
    -Dglew-cmake_BUILD_STATIC:BOOL=OFF
)
add_external_project(glew vendored/glew OFF "" "${GLEW_ARGS}" ON)

if(MSVC)
  # This is missing in the current source code for the SDL2 mirror we
  # are using.
  set(SDL2_ARGS
      -DCMAKE_SHARED_LINKER_FLAGS="vcruntime.lib"
  )
endif(MSVC)
add_external_project(sdl2 vendored/sdl2 OFF "" "${SDL2_ARGS}" OFF)

ExternalProject_Add( OSVR-RenderManager
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendored/OSVR-RenderManager
    BUILD_ALWAYS 1
    DOWNLOAD_COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive vendor/vrpn
    DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/vendored/OSVR-RenderManager
    CMAKE_ARGS
      ${cmake_common_args}
    INSTALL_DIR ${CMAKE_BINARY_DIR}/INSTALL
    DEPENDS OSVR-Core jsoncpp glew submodule_init sdl2
)

ExternalProject_Add( OSVRInstaller
    SOURCE_DIR ${CMAKE_SOURCE_DIR}
    BUILD_ALWAYS 1
    CMAKE_ARGS ${cmake_common_args}
    INSTALL_DIR ${CMAKE_BINARY_DIR}/INSTALL
    DEPENDS OSVR-RenderManager
)
