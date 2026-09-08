set(CMAKE_VERBOSE_MAKEFILE ON)

# ------------------------------------------------------------
# Target system
# ------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ------------------------------------------------------------
# Raspberry Pi cross compiler
# ------------------------------------------------------------

set(RPI_TOOLCHAIN "$ENV{HOME}/rpi-toolchain")
set(RPI_SYSROOT   "$ENV{HOME}/rpi-sysroot/rootfs")

set(CMAKE_C_COMPILER
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-gcc"
)

set(CMAKE_CXX_COMPILER
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-g++"
)

set(CMAKE_LINKER
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-ld"
)

set(CMAKE_AR
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-ar"
)

set(CMAKE_NM
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-nm"
)

set(CMAKE_OBJCOPY
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-objcopy"
)

set(CMAKE_OBJDUMP
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-objdump"
)

set(CMAKE_RANLIB
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-ranlib"
)

set(CMAKE_STRIP
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-strip"
)

set(CMAKE_SIZE
    "${RPI_TOOLCHAIN}/bin/aarch64-linux-gnu-size"
)

# ------------------------------------------------------------
# Raspberry Pi sysroot
#
# This is the filesystem copied from the actual Pi.
# ------------------------------------------------------------

set(CMAKE_SYSROOT "${RPI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${RPI_SYSROOT}")

# Raspberry Pi 64-bit Debian multiarch library directory
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

set(RPI_LIBRARY_DIR
    "${RPI_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
)

# ------------------------------------------------------------
# Compiler / linker flags
# ------------------------------------------------------------

set(CMAKE_C_FLAGS_INIT
    "-fPIC"
)

set(CMAKE_CXX_FLAGS_INIT
    "-fPIC"
)

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-Wl,-rpath-link,${RPI_LIBRARY_DIR} -L${RPI_LIBRARY_DIR}"
)

set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-Wl,-rpath-link,${RPI_LIBRARY_DIR} -L${RPI_LIBRARY_DIR}"
)

# ------------------------------------------------------------
# pkg-config
#
# Make pkg-config find libraries inside the Raspberry Pi
# sysroot rather than the Ubuntu host.
# ------------------------------------------------------------

set(ENV{PKG_CONFIG_SYSROOT_DIR}
    "${RPI_SYSROOT}"
)

set(ENV{PKG_CONFIG_LIBDIR}
    "${RPI_LIBRARY_DIR}/pkgconfig:${RPI_SYSROOT}/usr/share/pkgconfig"
)

set(ENV{PKG_CONFIG_PATH} "")

# ------------------------------------------------------------
# CMake find behavior
# ------------------------------------------------------------

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)