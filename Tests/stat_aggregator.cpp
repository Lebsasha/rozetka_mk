#include <cmath>
#include <cassert>
#include <algorithm>
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
            {15, 2.131}
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
