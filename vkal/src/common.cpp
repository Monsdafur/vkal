#include "common.hpp"

#include <print>

namespace vkal {

void debug(const std::string& message) {
    std::println("INFO   | {}", message);
}

} // namespace vkal
