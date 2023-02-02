#ifndef HYPERION_V2_LIB_SPAN_HPP
#define HYPERION_V2_LIB_SPAN_HPP

#include <Types.hpp>

namespace hyperion {

template <class T>
struct Span
{
    T *ptr;
    SizeType size;

    Span()
        : ptr(nullptr),
          size(0)
    {
    }

    Span(T *ptr, SizeType size)
        : ptr(ptr),
          size(size)
    {
    }

    template <SizeType Size>
    Span(const T (&ptrs)[Size])
        : ptr(&ptrs[0]),
          size(Size)
    {
    }
};

} // namespace hyperion

#endif