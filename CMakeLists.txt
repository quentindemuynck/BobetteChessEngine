cmake_minimum_required(VERSION 3.20)

project(BobetteChessEngine
    VERSION 0.1.0
    DESCRIPTION "Chess engine: FEN in, best move (UCI notation) out"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(ENGINE_ENABLE_ASAN    "Build with AddressSanitizer (/fsanitize=address)" OFF)
option(ENGINE_BUILD_TESTS    "Build the doctest unit tests"                     ON)
option(ENGINE_BUILD_WS_BRIDGE "Build the websocket bridge (engine_ws)"          ON)

# -----------------------------------------------------------------------------
# AddressSanitizer
# Applied globally (dependencies included): with MSVC, mixing ASan and non-ASan
# object files that use the STL triggers /failifmismatch linker errors.
# -----------------------------------------------------------------------------
if(ENGINE_ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address)
        # ASan is incompatible with /RTC1 and incremental linking.
        foreach(flag_var CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG)
            string(REPLACE "/RTC1" "" ${flag_var} "${${flag_var}}")
        endforeach()
        add_link_options(/INCREMENTAL:NO)
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

# -----------------------------------------------------------------------------
# Link-time optimization (Release / RelWithDebInfo only)
# The release preset turns on CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE;
# here we only check that the toolchain actually supports it.
# -----------------------------------------------------------------------------
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION OR CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output LANGUAGES CXX)
    if(NOT ipo_supported)
        message(WARNING "LTO requested but not supported: ${ipo_output}")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    endif()
endif()

# -----------------------------------------------------------------------------
# Warning flags for OUR code only.
# These are attached per-target via engine_set_warnings(), never through
# CMAKE_CXX_FLAGS or add_compile_options(), so dependencies are unaffected.
# -----------------------------------------------------------------------------
function(engine_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
            /external:anglebrackets   # treat <...> includes as external headers
            /external:W0              # ...and silence warnings in them
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Dependencies (all pinned)
# -----------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Mark dependency headers as SYSTEM so /W4 doesn't fire on them (CMake >= 3.25).
set(ENGINE_FETCH_SYSTEM "")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
    set(ENGINE_FETCH_SYSTEM SYSTEM)
endif()

# --- chess-library (header-only) ----------------------------------------------
# The repo has no release tags, so it is pinned to a commit on master
# (2026-11-03, "fix: inaccurate repetition detection (#252)").
FetchContent_Declare(chess_library
    GIT_REPOSITORY https://github.com/Disservin/chess-library.git
    GIT_TAG        430266434aeb42f19bb5baa84597d671824889ff
    ${ENGINE_FETCH_SYSTEM}
)
FetchContent_MakeAvailable(chess_library)

# chess-library ships no CMakeLists.txt, so we define its target ourselves.
add_library(chess_library INTERFACE)
add_library(chess::chess_library ALIAS chess_library)
target_include_directories(chess_library SYSTEM INTERFACE
    ${chess_library_SOURCE_DIR}/include)

# --- mbedTLS (TLS backend for IXWebSocket, needed for wss://) ---------------------
if(ENGINE_BUILD_WS_BRIDGE)
    set(ENABLE_PROGRAMS                     OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING                      OFF CACHE BOOL "" FORCE)
    set(MBEDTLS_FATAL_WARNINGS              OFF CACHE BOOL "" FORCE)
    set(DISABLE_PACKAGE_CONFIG_AND_INSTALL  ON  CACHE BOOL "" FORCE)

    FetchContent_Declare(mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG        v3.6.7
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES framework   # required by mbedTLS 3.6 CMake
        ${ENGINE_FETCH_SYSTEM}
    )
    FetchContent_MakeAvailable(mbedtls)

    # Hand the targets to IXWebSocket instead of letting it find_package() a
    # system-installed mbedTLS (there is none on a normal Windows machine).
    set(MBEDTLS_FOUND        TRUE)
    set(MBEDTLS_INCLUDE_DIRS ${mbedtls_SOURCE_DIR}/include)
    set(MBEDTLS_LIBRARIES    mbedtls mbedx509 mbedcrypto)
endif()

# --- IXWebSocket -----------------------------------------------------------------
if(ENGINE_BUILD_WS_BRIDGE)
    set(USE_TLS             ON  CACHE BOOL "" FORCE)   # wss:// for the public server
    set(USE_MBED_TLS        ON  CACHE BOOL "" FORCE)   # same TLS backend on every OS
    set(USE_ZLIB            OFF CACHE BOOL "" FORCE)   # no zlib dependency on Windows
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_DEMO          OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG        v12.1.0
        GIT_SHALLOW    TRUE
        ${ENGINE_FETCH_SYSTEM}
    )
    FetchContent_MakeAvailable(ixwebsocket)

    # IXWebSocket only sets this when it finds mbedTLS itself; we provided it.
    target_compile_definitions(ixwebsocket PRIVATE IXWEBSOCKET_USE_MBED_TLS_MIN_VERSION_3)
endif()

# --- doctest ---------------------------------------------------------------------
if(ENGINE_BUILD_TESTS)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.5.3
        GIT_SHALLOW    TRUE
        ${ENGINE_FETCH_SYSTEM}
    )
    FetchContent_MakeAvailable(doctest)
endif()

# -----------------------------------------------------------------------------
# Our targets
# -----------------------------------------------------------------------------
add_subdirectory(engine)
add_subdirectory(uci)

if(ENGINE_BUILD_WS_BRIDGE)
    add_subdirectory(ws_bridge)
endif()

if(ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
