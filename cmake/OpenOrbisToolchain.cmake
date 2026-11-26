# CMake toolchain file for the OpenOrbis PS4 Toolchain.
#
#   export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
#   cmake -S . -B build/ps4 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/OpenOrbisToolchain.cmake
#   cmake --build build/ps4
#
# It mirrors the compile flags used by the Makefiles of the official OpenOrbis samples.

if(NOT OO_PS4_TOOLCHAIN)
  set(OO_PS4_TOOLCHAIN "$ENV{OO_PS4_TOOLCHAIN}")
endif()
if(NOT OO_PS4_TOOLCHAIN OR NOT EXISTS "${OO_PS4_TOOLCHAIN}/link.x")
  message(FATAL_ERROR
    "OO_PS4_TOOLCHAIN must point to the OpenOrbis toolchain root (the folder that contains link.x, "
    "include/, lib/ and bin/). Current value: '${OO_PS4_TOOLCHAIN}'")
endif()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(OO_CLANG   NAMES clang)
find_program(OO_CLANGXX NAMES clang++)
find_program(OO_LD      NAMES ld.lld lld)
if(NOT OO_CLANG OR NOT OO_CLANGXX OR NOT OO_LD)
  message(FATAL_ERROR "clang, clang++ and ld.lld are required (e.g. apt install clang lld).")
endif()

set(CMAKE_C_COMPILER   "${OO_CLANG}")
set(CMAKE_CXX_COMPILER "${OO_CLANGXX}")
set(CMAKE_C_COMPILER_TARGET   x86_64-pc-freebsd12-elf)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-freebsd12-elf)

# We link with ld.lld ourselves; CMake's compiler checks must not try to link an executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_oo_common "-fPIC -funwind-tables -isysroot ${OO_PS4_TOOLCHAIN} -isystem ${OO_PS4_TOOLCHAIN}/include")
set(CMAKE_C_FLAGS_INIT   "${_oo_common}")
set(CMAKE_CXX_FLAGS_INIT "${_oo_common} -isystem ${OO_PS4_TOOLCHAIN}/include/c++/v1 -fno-exceptions -fno-rtti")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
