cmake_minimum_required(VERSION 3.16)
project(ps5_emulator_ui_for_ps4 VERSION 1.0.0 LANGUAGES CXX)

option(PS5UI_BUILD_HOST_TESTS "Build the desktop test runner instead of the PS4 target" OFF)
option(PS5UI_ENABLE_LAUNCH    "Experimental: try to launch other titles (privilege/firmware dependent)" OFF)
set(PS5UI_TITLE_ID   "PSUI00001"                            CACHE STRING "9-character title id (must match pkg/sce_sys/param.sfo)")
set(PS5UI_CONTENT_ID "IV0000-PSUI00001_00-0000000000000000" CACHE STRING "36-character content id (must match param.sfo)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PS5UI_CORE_SOURCES
  src/core/AdaptiveResolution.cpp
  src/core/FileSystem.cpp
  src/core/FpsCounter.cpp
  src/core/Input.cpp
  src/core/Library.cpp
  src/core/Log.cpp
  src/core/Settings.cpp
  src/core/Sfo.cpp
  src/core/Upscaler.cpp
  src/gfx/Background.cpp
  src/gfx/FontData.cpp
  src/gfx/Png.cpp
  src/gfx/Renderer.cpp
  src/ui/Screens.cpp
  src/ui/UiApp.cpp
  src/ui/Widgets.cpp)

# ---------------------------------------------------------------- desktop tests
if(PS5UI_BUILD_HOST_TESTS)
  enable_testing()
  add_executable(ps5ui_tests ${PS5UI_CORE_SOURCES} src/platform/host/HostPlatform.cpp tests/test_main.cpp)
  target_include_directories(ps5ui_tests PRIVATE src ${CMAKE_SOURCE_DIR})
  target_compile_options(ps5ui_tests PRIVATE -Wall -Wextra)
  add_test(NAME ps5ui_tests
           COMMAND ps5ui_tests "${CMAKE_BINARY_DIR}/png" "${CMAKE_SOURCE_DIR}/pkg/sce_sys"
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  return()
endif()

# ------------------------------------------------------------------ PS4 target
if(NOT OO_PS4_TOOLCHAIN)
  message(FATAL_ERROR
    "Configure with the OpenOrbis toolchain file:\n"
    "  cmake -S . -B build/ps4 -DCMAKE_TOOLCHAIN_FILE=cmake/OpenOrbisToolchain.cmake\n"
    "(or use -DPS5UI_BUILD_HOST_TESTS=ON for the desktop test runner).")
endif()

# System libraries (stub libraries shipped by the toolchain).
set(OO_REQUIRED_LIBS c c++ kernel SceVideoOut ScePad SceUserService)
set(OO_OPTIONAL_LIBS "")
if(PS5UI_ENABLE_LAUNCH)
  list(APPEND OO_REQUIRED_LIBS SceSystemService)
endif()

set(OO_LINK_LIBS "")
foreach(lib IN LISTS OO_REQUIRED_LIBS)
  file(GLOB _found "${OO_PS4_TOOLCHAIN}/lib/lib${lib}.*")
  if(NOT _found)
    file(GLOB _avail "${OO_PS4_TOOLCHAIN}/lib/*")
    string(REPLACE "${OO_PS4_TOOLCHAIN}/lib/" "" _avail "${_avail}")
    message(FATAL_ERROR
      "Required library 'lib${lib}' was not found in ${OO_PS4_TOOLCHAIN}/lib.\n"
      "Available files: ${_avail}\n"
      "Adjust OO_REQUIRED_LIBS in CMakeLists.txt to the names your toolchain version uses.")
  endif()
  set(OO_LINK_LIBS "${OO_LINK_LIBS} -l${lib}")
endforeach()

# Link with ld.lld exactly like the official OpenOrbis Makefiles do.
set(CMAKE_CXX_LINK_EXECUTABLE
  "${OO_LD} <OBJECTS> -o <TARGET> <LINK_LIBRARIES> -m elf_x86_64 -pie --script ${OO_PS4_TOOLCHAIN}/link.x --eh-frame-hdr -L${OO_PS4_TOOLCHAIN}/lib ${OO_LINK_LIBS} ${OO_PS4_TOOLCHAIN}/lib/crt1.o")

add_executable(ps5_emulator_ui ${PS5UI_CORE_SOURCES} src/platform/ps4/Ps4Platform.cpp src/main.cpp)
target_include_directories(ps5_emulator_ui PRIVATE src)
target_compile_definitions(ps5_emulator_ui PRIVATE PS5UI_PS4=1)
if(PS5UI_ENABLE_LAUNCH)
  target_compile_definitions(ps5_emulator_ui PRIVATE PS5UI_ENABLE_LAUNCH=1)
endif()
target_compile_options(ps5_emulator_ui PRIVATE -Wall -Wextra)
set_target_properties(ps5_emulator_ui PROPERTIES SUFFIX ".elf")

# ELF -> signed-fake eboot.bin -> param.sfo/icon -> GP4 -> PKG (see scripts/package_pkg.sh)
add_custom_command(TARGET ps5_emulator_ui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E env
          "OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN}"
          "PS5UI_CONTENT_ID=${PS5UI_CONTENT_ID}"
          bash "${CMAKE_SOURCE_DIR}/scripts/package_pkg.sh"
          "$<TARGET_FILE:ps5_emulator_ui>" "${CMAKE_BINARY_DIR}/dist"
  COMMENT "Packaging PKG"
  VERBATIM)
