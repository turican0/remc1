cmake_minimum_required(VERSION 3.21)

project(remc1 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(REMC1_ASAN "Build with AddressSanitizer" OFF)
option(REMC1_BUILD_TESTS "Build the remc1-tests GoogleTest target" ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "REMC1 builds for 64-bit targets only (x64, arm64); see the presets in CMakePresets.json.")
endif()

find_package(SDL2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG QUIET)
if(NOT SDL2_mixer_FOUND)
    # packages without the CMake config files: pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET GLOBAL SDL2_mixer)
    add_library(SDL2_mixer::SDL2_mixer ALIAS PkgConfig::SDL2_MIXER)
endif()

# ---------------------------------------------------------------------------
# Compiler settings shared by every target
# ---------------------------------------------------------------------------
add_library(remc1_options INTERFACE)
target_include_directories(remc1_options INTERFACE ${CMAKE_SOURCE_DIR})
target_compile_definitions(remc1_options INTERFACE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
if(MSVC)
    # /bigobj: sub_main.cpp is ~110k lines. /wd4996: POSIX/CRT names. /permissive-: off, decompiled code relies on MSVC leniency.
    target_compile_options(remc1_options INTERFACE /utf-8 /bigobj /Zc:__cplusplus /wd4996 /MP)
    # Debug: keep stack-frame checks but not /RTCu - Hex-Rays code routinely "returns" uninitialised
    # locals (the original just returned whatever was in eax) and /RTCu aborts on that.
    if(NOT REMC1_ASAN)
        foreach(flags CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
            string(REPLACE "/RTC1" "/RTCs" ${flags} "${${flags}}")
        endforeach()
    endif()
    if(REMC1_ASAN)
        # MSVC ASan is incompatible with /RTC1 and incremental linking; the runtime DLL is copied
        # next to the executables below.
        foreach(flags CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
            string(REPLACE "/RTC1" "" ${flags} "${${flags}}")
        endforeach()
        target_compile_options(remc1_options INTERFACE /fsanitize=address /Zi)
        target_link_options(remc1_options INTERFACE /INCREMENTAL:NO /DEBUG)
        get_filename_component(REMC1_MSVC_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(REMC1_ASAN_DLL "${REMC1_MSVC_BIN}/clang_rt.asan_dynamic-x86_64.dll")
    endif()
else()
    # -U_FORTIFY_SOURCE: Ubuntu's GCC enables it by default; the decompiled code writes across field
    # boundaries of the packed DOS structs on purpose (e.g. a 12-character name plus its 0 into a 12-byte
    # field, the 0 landing in the next field, as in the original), which fortify aborts on.
    target_compile_options(remc1_options INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:-fpermissive> -Wno-narrowing -fno-strict-aliasing -fwrapv -U_FORTIFY_SOURCE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        target_compile_options(remc1_options INTERFACE -fsigned-char)
    endif()
    if(REMC1_ASAN)
        target_compile_options(remc1_options INTERFACE -fsanitize=address,undefined)
        target_link_options(remc1_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

# ---------------------------------------------------------------------------
# Engine: all game + platform code, as a static library so tests can link it
# ---------------------------------------------------------------------------
set(REMC1_ENGINE_SOURCES
    sub_main.cpp
    engine/Basic.cpp
    engine/ptr32.cpp
    engine/CommandLineParser.cpp
    portability/fcaseopen.cpp
    portability/port_filesystem.cpp
    portability/port_outputs.cpp
    portability/port_sdl_vga_mouse.cpp
    portability/port_time.cpp
    portability/Settings.cpp
    portability/InputScript.cpp
    portability/port_timer.cpp
    portability/port_watchdog.cpp
    portability/SdlPresenter.cpp
    portability/port_sound.cpp
    portability/MemSnapshot.cpp
    utilities/Compare.cpp
    utilities/DataFileIO.cpp
)
if(WIN32)
    list(APPEND REMC1_ENGINE_SOURCES portability/dirent.c)
endif()

add_library(remc1_engine STATIC ${REMC1_ENGINE_SOURCES})
target_link_libraries(remc1_engine PUBLIC remc1_options
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    nlohmann_json::nlohmann_json)
target_compile_definitions(remc1_engine PUBLIC SDL_MAIN_HANDLED)

# ---------------------------------------------------------------------------
# Game executable
# ---------------------------------------------------------------------------
add_executable(remc1 REMC1.cpp)
target_link_libraries(remc1 PRIVATE remc1_engine)
if(MSVC)
    set_property(TARGET remc1 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:remc1>")
endif()

# Runtime assets next to the executable
add_custom_command(TARGET remc1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:remc1>
    VERBATIM)
if(REMC1_ASAN_DLL)
    add_custom_command(TARGET remc1 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${REMC1_ASAN_DLL}" $<TARGET_FILE_DIR:remc1>
        VERBATIM)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(REMC1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
