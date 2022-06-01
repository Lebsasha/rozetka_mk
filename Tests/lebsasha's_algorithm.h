#ifndef LEBSASHA_S_ALGORITHM
#define LEBSASHA_S_ALGORITHM

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
