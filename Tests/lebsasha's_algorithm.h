#ifndef LEBSASHA_S_ALGORITHM
#define LEBSASHA_S_ALGORITHM

#include <ostream>
#include <algorithm>

namespace l_std
{
    template<typename Enumeration>
    constexpr std::enable_if_t<std::is_enum_v<Enumeration>, std::underlying_type_t<Enumeration>>
    enum_to_number(const Enumeration enum_member)
    {
        return static_cast<std::underlying_type_t<Enumeration>>(enum_member);
    }

    template<typename Enumeration>
    std::enable_if_t<std::is_enum_v<Enumeration>, std::ostream&>
    operator<<(std::ostream& ostream, const Enumeration enum_member)
    {
        return ostream << enum_to_number(enum_member);
    }
}

namespace l_std
{
    template<class InputIterator, class OutputIterator,
            class UnaryOperator, class Pred>
    OutputIterator transform_if(InputIterator first1, InputIterator last1,
                                OutputIterator result, UnaryOperator operation, Pred predicate)
    {
        while (first1 != last1)
        {
            if (predicate(*first1))
            {
                *result = operation(*first1);
                ++result;
            }
            ++first1;
        }
        return result;
    }
}

#endif //LEBSASHA_S_ALGORITHM
