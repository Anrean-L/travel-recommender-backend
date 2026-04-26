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

add_executable(scheduler_tests scheduler_tests.cpp)

target_include_directories(scheduler_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(scheduler_tests PRIVATE GTest::gtest_main)

set_target_properties(scheduler_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

include(GoogleTest)
gtest_discover_tests(scheduler_tests)

add_executable(scoring_tests scoring_tests.cpp)

target_include_directories(scoring_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(scoring_tests PRIVATE app_core GTest::gtest_main)

set_target_properties(scoring_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

gtest_discover_tests(scoring_tests)

add_executable(clients_tests clients_tests.cpp)

target_include_directories(clients_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(clients_tests PRIVATE app_core GTest::gtest_main)

set_target_properties(clients_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

gtest_discover_tests(clients_tests)
