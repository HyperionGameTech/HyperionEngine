#ifndef HYPERION_V2_LIB_INLINE_ALLOCATOR_HPP
#define HYPERION_V2_LIB_INLINE_ALLOCATOR_HPP

#include <core/lib/allocators/Allocator.hpp>

namespace hyperion {

template <class T, SizeType NumElements>
class InlineAllocator : public Allocator<InlineAllocator>
{
public:
    InlineAllocator()
        : m_elements(),
          m_index(0)
    {
    }

    template <class U, class ...Args>
    U *New(Args &&... args)
    {
        static_assert(sizeof(U) <= sizeof(T), "Size of U must be less than or equal to size of T");

        if (m_index >= NumElements) {
            return nullptr;
        }

        U *ptr = new (&m_elements[m_index++]) U(std::forward<Args>(args)...);
        return ptr;
    }

    template <class U>
    void Delete(U *ptr)
    {
        static_assert(sizeof(U) <= sizeof(T), "Size of U must be less than or equal to size of T");

        if (ptr == nullptr) {
            return;
        }

        ptr->~U();
    }

private:
    T           m_elements[NumElements];
    SizeType    m_index;
};

} // namespace hyperion

#endif