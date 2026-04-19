cmake_minimum_required(VERSION 3.12)

project(
    labwork7
    LANGUAGES CXX
)

set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.14.1)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

add_subdirectory(src)
enable_testing()
add_subdirectory(tests)

include(tests/coverage.cmake)

if(TARGET coverage AND TARGET unit_tests)
    add_dependencies(coverage unit_tests)
endif()
