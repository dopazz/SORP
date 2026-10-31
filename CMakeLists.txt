# CMakeLists.txt -- SORP, a Super Off Road port on SDL3
#
# Windows / MSVC
# --------------
# Download SDL3-devel-<version>-VC.zip from
#   https://github.com/libsdl-org/SDL/releases
# and extract it so you have:
#
#   <project>/
#     CMakeLists.txt
#     src/           sorp.c, og_*.c, offroad_gfx.c, *.h, and tp_*.c,
#                    the arcade Track-Pak's tracks
#     gamedata/      your own copy of the original game files
#     SDL3/
#         include/SDL3/SDL.h ...
#         lib/x64/SDL3.lib
#         lib/x64/SDL3.dll
#
# Then, from the project root:
#   cmake -S . -B build -A x64
#   cmake --build build --config Release
#
# SDL3.dll is copied next to the .exe automatically.
#
# If you put SDL somewhere else, pass its root:
#   cmake -S . -B build -A x64 -DSDL3_ROOT=C:/libs/SDL3
#
# Linux
# -----
#   sudo apt install libsdl3-dev     (or build SDL from source)
#   cmake -S . -B build && cmake --build build
# pkg-config is used automatically; SDL3_ROOT is ignored.

cmake_minimum_required(VERSION 3.16)
project(superoffroad C)

set(CMAKE_C_STANDARD 99)

set(SOURCES
    src/sorp.c
    src/offroad_gfx.c
    src/og_data.c
    src/og_course.c
    src/og_palette.c
    src/og_sound.c
    src/opl3.c          # Nuked OPL3, in place of src/og_opl2.c
    src/tp_rom.c        # the arcade Track-Pak, from the player's own ROMs
    src/tp_data.c
    src/tp_sound.c      # its sound board, from the same ROMs
)

add_executable(sorp ${SOURCES})
target_include_directories(sorp PRIVATE src)

# ---------------------------------------------------------------- SDL3

set(SDL3_ROOT "${CMAKE_SOURCE_DIR}/SDL3"
    CACHE PATH "Root of the SDL3 development package")

# Order matters. An explicitly supplied SDL3_ROOT always wins: otherwise
# find_package can silently pick up an unrelated SDL install (vcpkg, an
# old preview) and you get link errors for SDL3-only entry points such as
# SDL_SetRenderLogicalPresentation while the SDL2-compatible names -- which
# is most of them -- resolve fine and hide the problem.
if(EXISTS "${SDL3_ROOT}/include/SDL3/SDL.h")
    message(STATUS "SDL3: using ${SDL3_ROOT}")
    target_include_directories(sorp PRIVATE "${SDL3_ROOT}/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_sdl_libdir "${SDL3_ROOT}/lib/x64")
    else()
        set(_sdl_libdir "${SDL3_ROOT}/lib/x86")
    endif()
    if(NOT EXISTS "${_sdl_libdir}/SDL3.lib")
        message(FATAL_ERROR
            "Found SDL3 headers in ${SDL3_ROOT} but no SDL3.lib in "
            "${_sdl_libdir}. You may have extracted the *source* zip "
            "instead of SDL3-devel-<version>-VC.zip.")
    endif()
    target_link_libraries(sorp PRIVATE "${_sdl_libdir}/SDL3.lib")
    add_custom_command(TARGET sorp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_sdl_libdir}/SDL3.dll" "$<TARGET_FILE_DIR:sorp>")
else()
    # Require 3.2 explicitly so an ancient SDL3 preview fails here at
    # configure time rather than at link time.
    find_package(SDL3 3.2 CONFIG QUIET)
    if(SDL3_FOUND)
        message(STATUS "SDL3: CMake package ${SDL3_VERSION} from ${SDL3_DIR}")
        target_link_libraries(sorp PRIVATE SDL3::SDL3)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
        endif()
        if(TARGET PkgConfig::SDL3)
            message(STATUS "SDL3: pkg-config ${SDL3_VERSION}")
            target_link_libraries(sorp PRIVATE PkgConfig::SDL3)
        else()
            message(FATAL_ERROR
                "No SDL3 found.\n"
                "Windows: extract SDL3-devel-<version>-VC.zip so that\n"
                "  ${SDL3_ROOT}/include/SDL3/SDL.h exists,\n"
                "or pass -DSDL3_ROOT=<path>.\n"
                "Linux: install libsdl3-dev.")
        endif()
    endif()
endif()

# ------------------------------------------------------------- platform

if(MSVC)
    # fopen/snprintf are fine here; silence the CRT deprecation noise.
    target_compile_definitions(sorp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sorp PRIVATE /W3)
else()
    target_compile_options(sorp PRIVATE -Wall -Wextra)
    target_link_libraries(sorp PRIVATE m ${CMAKE_DL_LIBS})   # dlopen: the GPU presenter
endif()
