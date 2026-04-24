cmake_minimum_required(VERSION 3.20)
project(Silex VERSION 0.5.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_PYTHON "Build Python bindings" ON)
option(SILEX_PYTHON_WHEEL_LAYOUT "Install Python artifacts in a wheel-friendly layout" OFF)

# pybind11 - rez package or bundled
if(BUILD_PYTHON)
    find_package(pybind11 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11/CMakeLists.txt)
            add_subdirectory(third_party/pybind11)
            set(pybind11_FOUND TRUE)
            message(STATUS "Using bundled pybind11")
        else()
            message(STATUS "pybind11 not found, fetching via FetchContent...")
            FetchContent_Declare(pybind11
                GIT_REPOSITORY https://github.com/pybind/pybind11.git
                GIT_TAG        v2.13.6
                GIT_SHALLOW    TRUE
            )
            FetchContent_MakeAvailable(pybind11)
            set(pybind11_FOUND TRUE)
        endif()
    endif()
endif()

# nlohmann_json - rez package or FetchContent
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching via FetchContent...")
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# JsonCpp - rez package or FetchContent
find_package(jsoncpp CONFIG QUIET)
if(NOT jsoncpp_FOUND)
    message(STATUS "jsoncpp not found, fetching via FetchContent...")
    FetchContent_Declare(jsoncpp
        GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
        GIT_TAG        1.9.6
        GIT_SHALLOW    TRUE
    )
    set(JSONCPP_WITH_TESTS OFF CACHE INTERNAL "")
    set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE INTERNAL "")
    set(BUILD_OBJECT_LIBS OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(jsoncpp)

    # Wheel builds link JsonCpp statically into shared objects on Linux, so the
    # fetched archive must be compiled as position-independent code.
    if(TARGET jsoncpp_static)
        set_target_properties(jsoncpp_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# spdlog - FetchContent (header-only mode)
find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found, fetching via FetchContent...")
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.15.1
        GIT_SHALLOW    TRUE
    )
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "")
    set(SPDLOG_BUILD_TESTS OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(spdlog)
endif()

# json5cpp - header-only, depends on jsoncpp
FetchContent_Declare(json5cpp
    GIT_REPOSITORY https://github.com/mortie/json5cpp.git
    GIT_TAG        v1.1.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json5cpp)

# GTest for testing
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Install directories
include(GNUInstallDirs)
set(CMAKE_INSTALL_INCLUDEDIR include)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_BINDIR bin)

if(SILEX_PYTHON_WHEEL_LAYOUT)
    set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Install directory for runtime targets" FORCE)
    set(SILEX_PYTHON_MODULE_INSTALL_DIR ".")
    set(SILEX_PYTHON_RUNTIME_INSTALL_DIR ".")
else()
    set(SILEX_PYTHON_MODULE_INSTALL_DIR "python")
    set(SILEX_PYTHON_RUNTIME_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
endif()

message(STATUS "===========================================")
message(STATUS "  Silex Library Configuration")
message(STATUS "===========================================")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Build Python: ${BUILD_PYTHON}")
message(STATUS "  Wheel Layout: ${SILEX_PYTHON_WHEEL_LAYOUT}")
message(STATUS "  pybind11: ${pybind11_FOUND}")
message(STATUS "===========================================")

# Core library
add_subdirectory(source/silex_core)

# Python bindings
if(BUILD_PYTHON AND pybind11_FOUND)
    add_subdirectory(source/silex_core/bindings)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
