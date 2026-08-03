#[[
  FindPDFium.cmake

  Downloads pre-built PDFium binaries from
      https://github.com/bblanchon/pdfium-binaries
  extracts them, and exposes an imported target `PDFium::PDFium`.

  --------------------------------------------------------------------------
  Cache variables (set BEFORE calling find_package(PDFium)):

    PDFIUM_VERSION   Release tag to fetch, e.g. "chromium/7947", or "latest".
                      Default: "latest"

    PDFIUM_PLATFORM   One of:
                        win | linux | linux-musl | mac
                        android | ios | ios-simulator | ios-catalyst
                      Default: auto-detected from CMAKE_SYSTEM_NAME, plus:
                        - glibc vs musl detected at configure time via `ldd
                          --version` (falls back to /lib/ld-musl-*.so.1)
                        - iOS device/simulator/Catalyst detected via the
                          PLATFORM / CMAKE_OSX_SYSROOT variables set by
                          toolchains such as ios.toolchain.cmake

    PDFIUM_ARCH       One of: x64 | x86 | arm | arm64
                      Default: auto-detected using the most reliable signal
                      for the active toolchain:
                        - Android:      CMAKE_ANDROID_ARCH_ABI
                        - MSVC/VS gen.: CMAKE_VS_PLATFORM_NAME / CMAKE_GENERATOR_PLATFORM
                        - Apple:        CMAKE_OSX_ARCHITECTURES
                        - otherwise:    CMAKE_SYSTEM_PROCESSOR

    PDFIUM_USE_V8     ON/OFF - download the V8/XFA-enabled build instead of
                      the plain one. Default: OFF

  Result variables:
    PDFium_FOUND
    PDFium_INCLUDE_DIRS
    PDFium_LIBRARIES
    PDFium_ROOT_DIR       (root of the extracted archive)

  Imported target:
    PDFium::PDFium        (SHARED IMPORTED)
--------------------------------------------------------------------------]]

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# 1. Release / variant selection
# ---------------------------------------------------------------------------
set(PDFIUM_VERSION "latest" CACHE STRING
        "pdfium-binaries release tag (e.g. chromium/7947) or 'latest'")

option(PDFIUM_USE_V8 "Download the V8/XFA-enabled PDFium build" OFF)

# ---------------------------------------------------------------------------
# 2. Platform auto-detection (override by setting -DPDFIUM_PLATFORM=... )
#    Includes a runtime check to tell glibc apart from musl (e.g. Alpine)
#    and distinguishes iOS device / simulator / Catalyst builds.
# ---------------------------------------------------------------------------
function(_pdfium_host_is_musl OUT_VAR)
    set(${OUT_VAR} FALSE PARENT_SCOPE)
    find_program(_pdfium_ldd_exe ldd)
    if(_pdfium_ldd_exe)
        execute_process(
                COMMAND ${_pdfium_ldd_exe} --version
                OUTPUT_VARIABLE _pdfium_ldd_out
                ERROR_VARIABLE  _pdfium_ldd_out
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_pdfium_ldd_out MATCHES "musl")
            set(${OUT_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endif()
    # Fallback: musl systems (e.g. Alpine) ship a loader at this well-known path
    if(EXISTS "/lib/ld-musl-x86_64.so.1" OR EXISTS "/lib/ld-musl-aarch64.so.1"
            OR EXISTS "/lib/ld-musl-armhf.so.1")
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    endif()
endfunction()

if(NOT DEFINED CACHE{PDFIUM_PLATFORM})
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(_pdfium_platform_default "win")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
        set(_pdfium_platform_default "android")

    elseif(CMAKE_SYSTEM_NAME MATCHES "iOS|tvOS|watchOS" OR DEFINED PLATFORM)
        # Cross-compiling with e.g. the ios.toolchain.cmake helper, which sets
        # a PLATFORM variable such as OS64 / SIMULATOR64 / SIMULATORARM64 / MAC_CATALYST
        if(DEFINED PLATFORM AND PLATFORM MATCHES "SIMULATOR")
            set(_pdfium_platform_default "ios-simulator")
        elseif(DEFINED PLATFORM AND PLATFORM MATCHES "CATALYST")
            set(_pdfium_platform_default "ios-catalyst")
        elseif(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
            set(_pdfium_platform_default "ios-simulator")
        else()
            set(_pdfium_platform_default "ios")
        endif()

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(_pdfium_platform_default "mac")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        _pdfium_host_is_musl(_pdfium_is_musl)
        if(_pdfium_is_musl)
            set(_pdfium_platform_default "linux-musl")
        else()
            set(_pdfium_platform_default "linux")
        endif()

    else()
        set(_pdfium_platform_default "linux")
    endif()
endif()
set(PDFIUM_PLATFORM "${_pdfium_platform_default}" CACHE STRING
        "pdfium-binaries platform: win|linux|linux-musl|mac|android|ios|ios-simulator|ios-catalyst")
set_property(CACHE PDFIUM_PLATFORM PROPERTY STRINGS
        win linux linux-musl mac android ios ios-simulator ios-catalyst)

# ---------------------------------------------------------------------------
# 3. Architecture auto-detection (override by setting -DPDFIUM_ARCH=... )
#    Uses the most reliable signal available for each toolchain:
#      - Android:        CMAKE_ANDROID_ARCH_ABI (armeabi-v7a/arm64-v8a/x86/x86_64)
#      - MSVC/VS gen.:   CMAKE_VS_PLATFORM_NAME / CMAKE_GENERATOR_PLATFORM
#      - Apple (mac/iOS):CMAKE_OSX_ARCHITECTURES, else uname-style processor
#      - everything else:CMAKE_SYSTEM_PROCESSOR
# ---------------------------------------------------------------------------
if(NOT DEFINED CACHE{PDFIUM_ARCH})

    if(CMAKE_SYSTEM_NAME STREQUAL "Android" AND DEFINED CMAKE_ANDROID_ARCH_ABI)
        if(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
            set(_pdfium_arch_default "arm")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
            set(_pdfium_arch_default "arm64")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
            set(_pdfium_arch_default "x86")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
            set(_pdfium_arch_default "x64")
        else()
            set(_pdfium_arch_default "arm64")
        endif()

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND (CMAKE_VS_PLATFORM_NAME OR CMAKE_GENERATOR_PLATFORM))
        # Visual Studio generators report the *target* arch here, which is
        # more reliable than CMAKE_SYSTEM_PROCESSOR (that reflects the host).
        if(CMAKE_VS_PLATFORM_NAME)
            set(_pdfium_vs_arch "${CMAKE_VS_PLATFORM_NAME}")
        else()
            set(_pdfium_vs_arch "${CMAKE_GENERATOR_PLATFORM}")
        endif()
        if(_pdfium_vs_arch MATCHES "^([Xx]64)$")
            set(_pdfium_arch_default "x64")
        elseif(_pdfium_vs_arch MATCHES "^(Win32|[Xx]86)$")
            set(_pdfium_arch_default "x86")
        elseif(_pdfium_vs_arch MATCHES "^(ARM64|arm64)$")
            set(_pdfium_arch_default "arm64")
        else()
            set(_pdfium_arch_default "x64")
        endif()

    elseif(CMAKE_OSX_ARCHITECTURES)
        # Explicit -DCMAKE_OSX_ARCHITECTURES=arm64|x86_64 wins on macOS/iOS/Catalyst
        list(GET CMAKE_OSX_ARCHITECTURES 0 _pdfium_osx_arch)
        if(_pdfium_osx_arch STREQUAL "arm64")
            set(_pdfium_arch_default "arm64")
        else()
            set(_pdfium_arch_default "x64")
        endif()

    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        set(_pdfium_arch_default "x64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
        set(_pdfium_arch_default "x86")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(_pdfium_arch_default "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv7|arm)$")
        set(_pdfium_arch_default "arm")
    else()
        set(_pdfium_arch_default "x64")
    endif()
endif()
set(PDFIUM_ARCH "${_pdfium_arch_default}" CACHE STRING
        "pdfium-binaries architecture: x64|x86|arm|arm64")
set_property(CACHE PDFIUM_ARCH PROPERTY STRINGS x64 x86 arm arm64)

message(STATUS "[PDFium] Auto-detected target: platform=${PDFIUM_PLATFORM} arch=${PDFIUM_ARCH}")

# ---------------------------------------------------------------------------
# 4. Compose asset name / download URL
#    Asset naming convention used by bblanchon/pdfium-binaries:
#      pdfium-<platform>-<arch>.tgz          (standard build)
#      pdfium-v8-<platform>-<arch>.tgz       (V8/XFA build)
# ---------------------------------------------------------------------------
if(PDFIUM_USE_V8)
    set(_pdfium_asset "pdfium-v8-${PDFIUM_PLATFORM}-${PDFIUM_ARCH}.tgz")
else()
    set(_pdfium_asset "pdfium-${PDFIUM_PLATFORM}-${PDFIUM_ARCH}.tgz")
endif()

if(PDFIUM_VERSION STREQUAL "latest")
    set(_pdfium_url
            "https://github.com/bblanchon/pdfium-binaries/releases/latest/download/${_pdfium_asset}")
else()
    set(_pdfium_url
            "https://github.com/bblanchon/pdfium-binaries/releases/download/${PDFIUM_VERSION}/${_pdfium_asset}")
endif()

set(PDFium_ROOT_DIR
        "${CMAKE_BINARY_DIR}/_deps/pdfium-${PDFIUM_PLATFORM}-${PDFIUM_ARCH}${PDFIUM_USE_V8}"
        CACHE PATH "Root directory of the extracted PDFium binaries")

set(_pdfium_download_dir "${CMAKE_BINARY_DIR}/_deps/downloads")
set(_pdfium_archive       "${_pdfium_download_dir}/${_pdfium_asset}")
set(_pdfium_stamp         "${PDFium_ROOT_DIR}/.extracted")

# ---------------------------------------------------------------------------
# 5. Download + extract (skipped if already done for this config)
# ---------------------------------------------------------------------------
if(NOT EXISTS "${_pdfium_stamp}")
    file(MAKE_DIRECTORY "${_pdfium_download_dir}")
    file(MAKE_DIRECTORY "${PDFium_ROOT_DIR}")

    message(STATUS "[PDFium] Downloading: ${_pdfium_url}")
    file(DOWNLOAD "${_pdfium_url}" "${_pdfium_archive}"
            STATUS   _pdfium_dl_status
            SHOW_PROGRESS
            TLS_VERIFY ON)

    list(GET _pdfium_dl_status 0 _pdfium_dl_code)
    if(NOT _pdfium_dl_code EQUAL 0)
        list(GET _pdfium_dl_status 1 _pdfium_dl_msg)
        file(REMOVE "${_pdfium_archive}")
        message(FATAL_ERROR
                "[PDFium] Download failed (${_pdfium_dl_code}): ${_pdfium_dl_msg}\n"
                "URL: ${_pdfium_url}\n"
                "Check PDFIUM_PLATFORM/PDFIUM_ARCH/PDFIUM_VERSION and your network settings.")
    endif()

    message(STATUS "[PDFium] Extracting to ${PDFium_ROOT_DIR}")
    file(ARCHIVE_EXTRACT INPUT "${_pdfium_archive}" DESTINATION "${PDFium_ROOT_DIR}")

    file(WRITE "${_pdfium_stamp}" "ok")
else()
    message(STATUS "[PDFium] Using cached binaries at ${PDFium_ROOT_DIR}")
endif()

# ---------------------------------------------------------------------------
# 6. Locate headers / libraries inside the extracted tree
# ---------------------------------------------------------------------------
find_path(PDFium_INCLUDE_DIR
        NAMES fpdfview.h
        PATHS "${PDFium_ROOT_DIR}/include"
        NO_DEFAULT_PATH)

if(PDFIUM_PLATFORM STREQUAL "win")
    # Windows ships an import lib (lib/pdfium.dll.lib) + runtime dll (bin/pdfium.dll)
    find_library(PDFium_IMPLIB
            NAMES pdfium pdfium.dll
            PATHS "${PDFium_ROOT_DIR}/lib"
            NO_DEFAULT_PATH)
    find_file(PDFium_SHARED_LIBRARY
            NAMES pdfium.dll
            PATHS "${PDFium_ROOT_DIR}/bin"
            NO_DEFAULT_PATH)
    set(PDFium_LIBRARIES "${PDFium_IMPLIB}")
else()
    # Linux/macOS/Android/iOS ship libpdfium.so / .dylib directly under lib/
    find_library(PDFium_SHARED_LIBRARY
            NAMES pdfium
            PATHS "${PDFium_ROOT_DIR}/lib"
            NO_DEFAULT_PATH)
    set(PDFium_LIBRARIES "${PDFium_SHARED_LIBRARY}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PDFium
        REQUIRED_VARS PDFium_INCLUDE_DIR PDFium_SHARED_LIBRARY
        VERSION_VAR PDFIUM_VERSION)

if(PDFium_FOUND AND NOT TARGET PDFium::PDFium)
    add_library(PDFium::PDFium SHARED IMPORTED GLOBAL)
    set_target_properties(PDFium::PDFium PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${PDFium_INCLUDE_DIR}")

    if(PDFIUM_PLATFORM STREQUAL "win")
        set_target_properties(PDFium::PDFium PROPERTIES
                IMPORTED_LOCATION "${PDFium_SHARED_LIBRARY}"
                IMPORTED_IMPLIB   "${PDFium_IMPLIB}")
    else()
        set_target_properties(PDFium::PDFium PROPERTIES
                IMPORTED_LOCATION "${PDFium_SHARED_LIBRARY}")
    endif()
endif()

set(PDFium_INCLUDE_DIRS "${PDFium_INCLUDE_DIR}")
mark_as_advanced(PDFium_INCLUDE_DIR PDFium_SHARED_LIBRARY PDFium_IMPLIB)
