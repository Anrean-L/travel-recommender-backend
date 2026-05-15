#pragma once

#include <iostream>

namespace app {

inline void LogInfo(const std::string& message) {
    std::cout << "[INFO] " << message << '\n';
}

inline void LogError(const std::string& message) {
    std::cerr << "[Error] " << message << '\n';
}

};  // namespace app
