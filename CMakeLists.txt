cmake_minimum_required(VERSION 3.20)

project(
    labwork9
    LANGUAGES CXX
)

include(FetchContent)

FetchContent_Declare(nlohmann_json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)

set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_ZSTD OFF CACHE BOOL "" FORCE)

if(MINGW)
    set(HTTPLIB_USE_NON_BLOCKING_GETADDRINFO OFF CACHE BOOL "" FORCE)
endif()

set(HTTPLIB_TEST OFF CACHE BOOL "" FORCE)
set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)
find_package(OpenSSL REQUIRED)

FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.26.0
)
FetchContent_MakeAvailable(nlohmann_json cpp_httplib)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
