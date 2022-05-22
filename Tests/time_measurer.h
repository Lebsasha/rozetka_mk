#ifndef TIME_MEASURER
#define TIME_MEASURER

#include <chrono>

class Time_measurer
{
    constexpr const static auto now = std::chrono::steady_clock::now;
    std::chrono::steady_clock::time_point t_begin;
    std::chrono::steady_clock::time_point t_end;
public:
    Time_measurer();

    void begin();

    void log_end(uint8_t cmd = 0, const std::string& description = "");
};

#endif //TIME_MEASURER
