add_library(app_core
    geolocation_client.cpp
    places_client.cpp 
    recommendation.cpp 
    routing_client.cpp 
    scoring.cpp 
    server.cpp 
    weather_client.cpp  
    http_client.cpp   
    config.cpp
)

target_compile_features(app_core PUBLIC cxx_std_23)

target_include_directories(app_core
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)


target_compile_definitions(app_core PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(app_core
    PUBLIC
        nlohmann_json::nlohmann_json
        httplib::httplib
        OpenSSL::SSL
        OpenSSL::Crypto
)

add_executable(app main.cpp)

target_link_libraries(app
    PRIVATE 
        app_core
)
