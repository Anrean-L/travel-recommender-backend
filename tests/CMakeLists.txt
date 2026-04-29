include(FetchContent)

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

add_executable(tests
    scheduler_tests.cpp
    scoring_tests.cpp
    clients_tests.cpp
    config_tests.cpp
    domain_tests.cpp
    domain_json_tests.cpp
    recommendation_tests.cpp
)

target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(tests PRIVATE app_core GTest::gtest_main)

set_target_properties(tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

gtest_discover_tests(tests)
