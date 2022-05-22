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

void Time_measurer::log_end(const uint8_t cmd, const std::__cxx11::string& description)
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
    if (std::chrono::duration_cast<std::chrono::seconds>(diff).count() >= 2)
        std::cout << std::chrono::duration_cast<std::chrono::seconds>(diff).count() << " s";
    else if (std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() >= 2)
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
    else
        std::cout << std::chrono::duration_cast<std::chrono::microseconds>(diff).count() << " us";
    std::cout << std::endl;
}
