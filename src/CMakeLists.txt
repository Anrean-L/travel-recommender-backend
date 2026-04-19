add_executable(app main.cpp)

target_include_directories(app PUBLIC ${PROJECT_SOURCE_DIR}/include)

set_target_properties(app PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
