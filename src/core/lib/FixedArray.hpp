#ifndef HYPERION_V2_LIB_FIXED_ARRAY_H
#define HYPERION_V2_LIB_FIXED_ARRAY_H

#include "ContainerBase.hpp"
#include <math/MathUtil.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

namespace containers {
namespace detail {

template <class T, SizeType Sz>
class FixedArray;

template <class T, SizeType Sz>
class FixedArrayImpl;

} // namespace detail
} // namespace containers

// #define FIXED_ARRAY_IMPL_METHOD(method_name, ...) \
//     containers::detail::FixedArrayImpl(&m_values[0]).method_name(__VA_ARGS__)

template <class T, SizeType Sz>
class FixedArray //
{
public:
    T m_values[MathUtil::Max(Sz, 1)];

    using Iterator = T *;
    using ConstIterator = const T *;

    using KeyType = UInt;

    static constexpr SizeType size = Sz;

private:

//     template <class Method, class ...Args>
//     auto ImplMethod(Method method, Args &&... args)
//     {
//         containers::detail::FixedArrayImpl<T, Sz> impl(&m_values[0]);
//         return impl.*method(std::forward<Args>(args)...);
//     }


// #define FIXED_ARRAY_IMPL_METHOD(method_name, ...) \
//     ImplMethod(&containers::detail::FixedArrayImpl<T, Sz>::method_name __VA_OPT__(,) __VA_ARGS__)


public:

    // FixedArray();

    // template <class ... Args>
    // constexpr FixedArray(Args &&... args)
    //     : m_values { std::forward<Args>(args)... }
    // {
    // }

    // constexpr FixedArray(const T *ary, SizeType count)
    // {
    //     for (SizeType i = 0; i < MathUtil::Min(Sz, count); i++) {
    //         m_values = ary[i];
    //     }
    // }

    // FixedArray(const FixedArray &other);
    // FixedArray &operator=(const FixedArray &other);
    // FixedArray(FixedArray &&other) noexcept;
    // FixedArray &operator=(FixedArray &&other) noexcept;
    // ~FixedArray();

    template <class Function>
    FixedArray Map(Function &&fn) const
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        return impl.Map(std::forward<Function>(fn));
    }

    [[nodiscard]] bool Contains(const T &value) const
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        return impl.Contains(value);
    }

    HYP_FORCE_INLINE
    T &operator[](SizeType index)
        { return m_values[index]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &operator[](SizeType index) const
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
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        return impl.Any(std::forward<Lambda>(lambda));
    }

    template <class Lambda>
    [[nodiscard]] bool Every(Lambda &&lambda) const
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        return impl.Every(std::forward<Lambda>(lambda));
    }

    [[nodiscard]] auto Sum() const
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        return impl.Sum();
    }

    [[nodiscard]] auto Avg() const
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        return impl.Avg();
    }

    template <class ConstIterator>
    [[nodiscard]] KeyType IndexOf(ConstIterator iter) const
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        return impl.IndexOf(iter);
    }

    template <class TaskSystem, class Lambda>
    void ParallelForEach(TaskSystem &task_system, Lambda &&lambda)
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(const_cast<T *>(&m_values[0]));
        impl.ParallelForEach(task_system, std::forward<Lambda>(lambda));
    }

    [[nodiscard]] HYP_FORCE_INLINE
    T *Data()
        { return static_cast<T *>(m_values); }

    [[nodiscard]] HYP_FORCE_INLINE
    const T *Data() const
        { return static_cast<const T *>(m_values); }

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
    
    [[nodiscard]] HashCode GetHashCode() const
    {
        containers::detail::FixedArrayImpl<T, Sz> impl(&m_values[0]);
        return impl.GetHashCode();
    }
};

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::FixedArray()
//     : m_values{}
// {
// }

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::FixedArray(const FixedArray &other)
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = other.m_values[i];
//     }
// }

// template <class T, SizeType Sz>
// auto FixedArray<T, Sz>::operator=(const FixedArray &other) -> FixedArray&
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = other.m_values[i];
//     }

//     return *this;
// }

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::FixedArray(FixedArray &&other) noexcept
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = std::move(other.m_values[i]);
//     }
// }

// template <class T, SizeType Sz>
// auto FixedArray<T, Sz>::operator=(FixedArray &&other) noexcept -> FixedArray&
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = std::move(other.m_values[i]);
//     }

//     return *this;
// }

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::~FixedArray() = default;

// deduction guide
template <typename Tp, typename ...Args>
FixedArray(Tp, Args...) -> FixedArray<std::enable_if_t<(std::is_same_v<Tp, Args> && ...), Tp>, 1 + sizeof...(Args)>;


namespace containers {
namespace detail {

template <class T, SizeType Sz>
class FixedArrayImpl : public ContainerBase<FixedArrayImpl<T, Sz>, UInt>
{
public:
    T *ptr;

    using Iterator = T *;
    using ConstIterator = const T *;

    FixedArrayImpl(T *ptr)
        : ptr(ptr)
    {
    }

    HYP_DEF_STL_BEGIN_END(ptr, ptr + Sz)
};

} // namespace detail
} // namespace containers

} // namespace hyperion

#endif
