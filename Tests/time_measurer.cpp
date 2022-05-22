#include <string>
#include <iostream>
#include "general.h"
#include "time_measurer.h"

Time_measurer::Time_measurer()
{
    begin();
}

void Time_measurer::begin()
{
    t_begin = now();
}

template<typename T, typename D>
typename T::rep convert_to(D point)
{
    return std::chrono::duration_cast<T>(point).count();
}

void Time_measurer::log_end(const uint8_t cmd, const std::string& description)
{
    t_end = now();
    std::cout << "Time ";
    if (cmd != 0 || !description.empty())
        std::cout << "for ";
    if (cmd != 0)
    {
        std::cout << print_cmd(cmd) << " command";
        if (!description.empty())
            std::cout << ", ";
    }
    if (!description.empty())
        std::cout << description;
    if (cmd != 0 || !description.empty())
        std::cout << " ";
    std::cout << "takes ";
    auto diff = t_end - t_begin;
    if (convert_to<std::chrono::minutes>(diff) >= 2)
    {
        auto minutes = convert_to<std::chrono::minutes>(diff);
        std::cout << minutes << " m, " << convert_to<std::chrono::seconds>(diff) - minutes * 60 << " s";
    }
    else if (convert_to<std::chrono::seconds>(diff) >= 2)
        std::cout << convert_to<std::chrono::seconds>(diff) << " s";
    else if (convert_to<std::chrono::milliseconds>(diff) >= 2)
        std::cout << convert_to<std::chrono::milliseconds>(diff) << " ms";
    else
        std::cout << convert_to<std::chrono::microseconds>(diff) << " us";
    std::cout << std::endl;
}
