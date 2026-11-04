# Cross-compile from Linux with the Arch mingw-w64 packages (or any toolchain
# exporting x86_64-w64-mingw32-*). Point MINGW_PREFIX at the sysroot when it is
# not /usr/x86_64-w64-mingw32.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX "/usr/x86_64-w64-mingw32" CACHE PATH "MinGW-w64 sysroot prefix")
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH "${MINGW_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# pkg-config must only see target libraries, never the host's /usr/lib.
set(ENV{PKG_CONFIG_LIBDIR}
    "${MINGW_PREFIX}/lib/pkgconfig:${MINGW_PREFIX}/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${MINGW_PREFIX}")
