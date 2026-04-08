#include "core/job_id.hpp"

#include <random>
#include <sstream>
#include <stdexcept>

JobId JobId::generate() {
    static std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 15);

    const char* hex = "0123456789abcdef";
    std::stringstream ss;

    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) ss << '-';
        ss << hex[dist(gen)];
    }

    return JobId(ss.str());
}

JobId JobId::fromString(const std::string& str) {
    if (!isValidUuid(str))
        throw std::invalid_argument("Invalid UUID: " + str);
    return JobId(str);
}

bool JobId::isValidUuid(const std::string& s) {
    if (s.size() != 36) return false;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return false;
        } else {
            char c = s[i];
            bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (!isHex) return false;
        }
    }
    return true;
}
