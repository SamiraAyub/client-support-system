#include "common/TimeUtils.h"

#include <array>
#include <chrono>

namespace csms {

std::string formatTimestampUtc(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::array<char, 32> buf{};
    std::strftime(buf.data(), buf.size(), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf.data());
}

std::string currentTimestampUtc() {
    const auto now = std::chrono::system_clock::now();
    return formatTimestampUtc(std::chrono::system_clock::to_time_t(now));
}

} // namespace csms
