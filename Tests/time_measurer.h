#ifndef TIME_MEASURER
#define TIME_MEASURER

#include <chrono>

class Time_measurer
{
    constexpr const static auto now = std::chrono::steady_clock::now;
    std::chrono::steady_clock::time_point t_begin = now();
    std::chrono::steady_clock::time_point t_end;
public:
    void begin();

    void log_end(uint8_t cmd = 0, const std::__cxx11::string& description = "");
};

#endif //TIME_MEASURER
