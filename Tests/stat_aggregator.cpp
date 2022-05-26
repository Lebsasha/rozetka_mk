// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <cmath>
#include <cassert>
#include <algorithm>
#include <iostream>
#include "stat_aggregator.h"

decltype(stat_aggregator::Student_distribution_coefficients) stat_aggregator::Student_distribution_coefficients = {
        {0.95,{
            {1, 12.706},
            {2, 4.303},
            {3, 3.182},
            {4, 2.776},
            {5, 2.571},
            {6, 2.447},
            {7, 3.365},
            {8, 2.306},
            {9, 2.262},
            {10, 2.228},
            {11, 2.201},
            {12, 2.179},
            {13, 2.160},
            {14, 2.145},
            {15, 2.131},
            {16, 2.120},
            {17, 2.110},
            {18, 2.101},
            {19, 2.093},
            {20, 2.086},
            {21, 2.080},
            {22, 2.074},
            {23, 2.069},
            {24, 2.064},
            {25, 2.060}
        }}
};

double stat_aggregator::get_confidence_interval(double std, size_t n, double confidence_level)
{
    if (n == 0)
        return 0;
    double stud_coef = get_student_distribution_coefficient(confidence_level, n);
    return stud_coef * std / sqrt(static_cast<double>(n));
}

double stat_aggregator::get_student_distribution_coefficient(double confidence_level, size_t n)
{
    assert(confidence_level > 0 && confidence_level < 1);

    for (auto &item : Student_distribution_coefficients)
    {
        if (std::abs(item.first - confidence_level) < std::numeric_limits<double>::epsilon())
        {
            size_t max_n_for_confidence_level = std::max_element(item.second.begin(), item.second.end())->first;
            if (n > max_n_for_confidence_level)
            {
                std::cerr << "In " << __PRETTY_FUNCTION__ << ": requested Student distribution coefficient' size parameter " << n
                        << "is higher than maximum known coefficient' parameter " << max_n_for_confidence_level << std::endl
                          << "Returning 0 instead " << std::endl;
                return 0;
            }
            auto iter = std::find_if(item.second.begin(), item.second.end(),
                                     [&](const auto& item) {
                                         return item.first == n;
                                     });
            if (iter != item.second.end())
                return iter->second;

            return 0; // We not found element, but for preventing program fail in assert, we return 0
        }
    }
//    assert(false);
    return 0;
}
