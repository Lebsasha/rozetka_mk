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

    std::chrono::seconds::rep log_end(uint8_t cmd, const std::string& description = "");
    std::chrono::seconds::rep log_end(const std::string& description = "");
//    [[nodiscard]] std::chrono::seconds::rep get_difference_in_seconds() const;
};

#endif //TIME_MEASURER
