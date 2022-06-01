#ifndef STAT_AGGREGATOR
#define STAT_AGGREGATOR

#include <map>

class stat_aggregator
{
public:

//    double get_mean();

    template<typename InputIterator, typename MeanType>
    static double get_std(InputIterator begin, InputIterator end, MeanType mean)
    {
        typedef typename InputIterator::value_type value_type;

        double variance = 0.0;
        std::for_each(begin, end, [&](value_type& d) {
            variance += (d - mean) * (d - mean);
        });
        size_t count = end - begin;
        double std;
        if (count != 0 && count != 1)
            std = sqrt(variance / static_cast<double>(count - 1));
        else
            std = std::numeric_limits<double>::quiet_NaN();
        return std;
    }

    template<typename InputIterator, typename MeanType, typename Func>
    static double get_std(InputIterator begin, InputIterator end, MeanType mean, Func mapper_to_number)
    {
        typedef typename InputIterator::value_type value_type;

        double std = 0.0;
        std::for_each(begin, end, [&](value_type& d) {
            std += (mapper_to_number(d) - mean) * (mapper_to_number(d) - mean);
        });
        size_t count = end - begin;
        if (count > 0)
            std = sqrt(std / static_cast<double>(count - 1));
        else
            std = std::numeric_limits<double>::quiet_NaN();
        return std;
    }

    // or (with definition in .cpp file)
//    std::vector<double> t;
//    double d = stat_aggregator::get_std(t.begin(), t.end(), 1);


    static double get_confidence_interval(double std, size_t n, double confidence_level);

    static double get_student_distribution_coefficient(double confidence_level, size_t n);

private:
    const static std::map<double, std::map<size_t, double>> Student_distribution_coefficients;

};

#endif //STAT_AGGREGATOR
