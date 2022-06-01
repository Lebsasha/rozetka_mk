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
            {25, 2.060},
            {26, 2.056},
            {27, 2.052},
            {28, 2.048},
            {29, 2.045},
            {30, 2.042},
            {40, 2.021},
            {60, 2.000},
            {80, 1.990},
            {100, 1.984},
            {200, 1.972},
            {500, 1.965}
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
    assert(n != 0);
    assert(confidence_level > 0 && confidence_level < 1);

    for (auto &conf_level_item : Student_distribution_coefficients)
    {
        if (std::abs(conf_level_item.first - confidence_level) < std::numeric_limits<double>::epsilon())
        {
            auto iter = std::find_if(conf_level_item.second.rbegin(), conf_level_item.second.rend(),
                                     [&](const auto& n_item) {
                                         return n_item.first <= n;
                                     });
            if (iter != conf_level_item.second.rend())
            {
                if (iter->first == n)
                    return iter->second;
                else
                {
                    std::cout << "Warining. In " << __PRETTY_FUNCTION__ << " Search n = " << n << " not found. "
                                                          "Returning known Student distribution coefficient for greatest n1 that n1 < n (n1 = " << iter->first << ")"
                                                          << "That will mean that your true confidence interval will be closer than it calculated" << std::endl;
                    return iter->second;
                }
            }


            return std::numeric_limits<double>::quiet_NaN(); // We not found element, but for preventing program fail in assert, we return nan
        }
    }
//    assert(false);
    return std::numeric_limits<double>::quiet_NaN();
}
