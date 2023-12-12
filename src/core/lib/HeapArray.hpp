#ifndef HYPERION_V2_LIB_HEAP_ARRAY_H
#define HYPERION_V2_LIB_HEAP_ARRAY_H

#include "ContainerBase.hpp"
#include <math/MathUtil.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

template <class T, SizeType Sz>
class HeapArray : public ContainerBase<HeapArray<T, Sz>, SizeType>
{
    T *m_values;

public:
    static constexpr Bool is_contiguous = true;

    using Base = ContainerBase<HeapArray<T, Sz>, SizeType>;

    using Iterator = T *;
    using ConstIterator = const T *;

    static constexpr SizeType size = Sz;

    HeapArray();
    HeapArray(const HeapArray &other);
    HeapArray &operator=(const HeapArray &other);
    HeapArray(HeapArray &&other) noexcept;
    HeapArray &operator=(HeapArray &&other) noexcept;
    ~HeapArray();

    template <class Function>
    HeapArray Map(Function &&fn) const
    {
        HeapArray result;
        SizeType index = 0;

        for (auto it = Begin(); it != End(); ++it) {
            result[index++] = fn(*it);
        }

        return result;
    }

    HYP_FORCE_INLINE
    T &operator[](typename HeapArray::Base::KeyType index)
        { return m_values[index]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &operator[](typename HeapArray::Base::KeyType index) const
        { return m_values[index]; }

    HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return Sz; }

    [[nodiscard]] HYP_FORCE_INLINE
    bool Empty() const
        { return Sz == 0; }

    [[nodiscard]] HYP_FORCE_INLINE
    bool Any() const
        { return Sz != 0; }

    template <class Lambda>
    [[nodiscard]] bool Any(Lambda &&lambda) const
        { return Base::Any(std::forward<Lambda>(lambda)); }

    template <class Lambda>
    [[nodiscard]] bool Every(Lambda &&lambda) const
        { return Base::Every(std::forward<Lambda>(lambda)); }

    [[nodiscard]] HYP_FORCE_INLINE
    T *Data()
        { return m_values; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T *Data() const
        { return m_values; }

    [[nodiscard]] HYP_FORCE_INLINE
    T &Front()
        { return m_values[0]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &Front() const
        { return m_values[0]; }

    [[nodiscard]] HYP_FORCE_INLINE
    T &Back()
        { return m_values[Sz - 1]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &Back() const
        { return m_values[Sz - 1]; }

    HYP_DEF_STL_BEGIN_END(&m_values[0], &m_values[Sz])
};

template <class T, SizeType Sz>
HeapArray<T, Sz>::HeapArray()
    : m_values(new T[Sz])
{
}

template <class T, SizeType Sz>
HeapArray<T, Sz>::HeapArray(const HeapArray &other)
    : m_values(new T[Sz])
{
    for (SizeType index = 0; index < other.Size(); index++) {
        m_values[index] = other.m_values[index];
    }
}

template <class T, SizeType Sz>
auto HeapArray<T, Sz>::operator=(const HeapArray &other) -> HeapArray&
{
    for (SizeType index = 0; index < other.Size(); index++) {
        m_values[index] = other.m_values[index];
    }

    return *this;
}

template <class T, SizeType Sz>
HeapArray<T, Sz>::HeapArray(HeapArray &&other) noexcept
    : m_values(new T[Sz])
{
    for (SizeType index = 0; index < other.Size(); index++) {
        m_values[index] = std::move(other.m_values[index]);
    }
}

template <class T, SizeType Sz>
auto HeapArray<T, Sz>::operator=(HeapArray &&other) noexcept -> HeapArray&
{
    for (SizeType index = 0; index < other.Size(); index++) {
        m_values[index] = std::move(other.m_values[index]);
    }

    return *this;
}

template <class T, SizeType Sz>
HeapArray<T, Sz>::~HeapArray()
{
    delete[] m_values;
}

} // namespace hyperion

#endif
