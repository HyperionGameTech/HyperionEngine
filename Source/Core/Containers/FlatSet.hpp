/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/ContainerBase.hpp>
#include <Core/Containers/SortedArray.hpp>

#include <Core/Utilities/Pair.hpp>

#include <Core/Utilities/Traits.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {
namespace containers {

/*! \brief TFlatSet is a sorted set container that stores unique elements in a flat contiguous array, based on the TSortedArray implementation.
 *  It provides fast lookup and insertion while maintaining order.
 *  \tparam T The type of elements stored in the flat set. */
template <class T, class AllocatorType = DynamicAllocator>
class TFlatSet : public TSortedArray<T, AllocatorType>
{
protected:
    using Base = TSortedArray<T, AllocatorType>;

public:
    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    using KeyType = T;
    using ValueType = T;

    using InsertResult = Pair<Iterator, bool>; // iterator, was inserted

    TFlatSet();

    TFlatSet(std::initializer_list<T> initializerList)
        : Base()
    {
        Reserve(initializerList.size());

        for (const auto& item : initializerList)
        {
            Insert(item);
        }
    }

    TFlatSet(const T* begin, const T* end)
        : Base()
    {
        if (begin && end)
        {
            const size_t dist = std::distance(begin, end);

            if (dist != 0)
            {
                Reserve(dist);

                for (const T* it = begin; it != end; ++it)
                {
                    Insert(*it);
                }
            }
        }
    }

    TFlatSet(const TFlatSet& other);
    TFlatSet& operator=(const TFlatSet& other);
    
    TFlatSet(TFlatSet&& other) noexcept;
    TFlatSet& operator=(TFlatSet&& other) noexcept;

    ~TFlatSet();

    Iterator Find(const T& value);
    ConstIterator Find(const T& value) const;

    template <class TFindAsType>
    HYP_FORCE_INLINE auto FindAs(const TFindAsType& value) -> Iterator
    {
        const auto it = TFlatSet<T, AllocatorType>::Base::LowerBound(value);

        if (it == End())
        {
            return it;
        }

        return (*it == value) ? it : End();
    }

    template <class TFindAsType>
    HYP_FORCE_INLINE auto FindAs(const TFindAsType& value) const -> ConstIterator
    {
        const auto it = TFlatSet<T, AllocatorType>::Base::LowerBound(value);

        if (it == End())
        {
            return it;
        }

        return (*it == value) ? it : End();
    }

    template <class TFindAsType>
    HYP_FORCE_INLINE bool Contains(const TFindAsType& value) const
    {
        return FindAs<TFindAsType>(value) != End();
    }

    InsertResult Insert(const T& value);
    InsertResult Insert(T&& value);

    template <class... Args>
    InsertResult Emplace(Args&&... args)
    {
        return Insert(T(std::forward<Args>(args)...));
    }

    Iterator Erase(ConstIterator it);
    Iterator Erase(const T& value);

    HYP_FORCE_INLINE size_t Size() const
    {
        return Base::Size();
    }

    HYP_FORCE_INLINE T* Data()
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE const T* Data() const
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return Base::Any();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return Base::Empty();
    }

    HYP_FORCE_INLINE void Clear()
    {
        Base::Clear();
    }

    HYP_FORCE_INLINE void Reserve(size_t size)
    {
        Base::Reserve(size);
    }

    HYP_FORCE_INLINE T& Front()
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE const T& Front() const
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE T& Back()
    {
        return Base::Back();
    }

    HYP_FORCE_INLINE const T& Back() const
    {
        return Base::Back();
    }

    template <class OtherContainerType>
    TFlatSet& Merge(const OtherContainerType& other)
    {
        for (const auto& item : other)
        {
            Insert(item);
        }

        return *this;
    }

    template <class OtherContainerType>
    TFlatSet& Merge(OtherContainerType&& other)
    {
        for (auto& item : other)
        {
            Insert(std::move(item));
        }

        other.Clear();

        return *this;
    }

    template <class OtherContainerType>
    TFlatSet Union(const OtherContainerType& other) const
    {
        TFlatSet result(*this);
        result.Merge(other);

        return result;
    }

    template <class OtherContainerType>
    TFlatSet Union(OtherContainerType&& other) const
    {
        TFlatSet result(*this);
        result.Merge(std::move(other));

        return result;
    }

    template <class OtherContainerType>
    TFlatSet Intersection(const OtherContainerType& other) const
    {
        TFlatSet result;

        for (auto it = Begin(); it != End(); ++it)
        {
            if (other.Contains(*it))
            {
                result.Insert(*it);
            }
        }

        return result;
    }

    HYP_NODISCARD HYP_FORCE_INLINE Array<T, AllocatorType> ToArray() const
    {
        return Array<T, AllocatorType>(Begin(), End());
    }

    HYP_DEF_STL_BEGIN_END(Base::Begin(), Base::End())
};

template <class T, class AllocatorType>
TFlatSet<T, AllocatorType>::TFlatSet()
    : Base()
{
}

template <class T, class AllocatorType>
TFlatSet<T, AllocatorType>::TFlatSet(const TFlatSet& other)
    : Base(other)
{
}

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::operator=(const TFlatSet& other) -> TFlatSet&
{
    Base::operator=(other);

    return *this;
}

template <class T, class AllocatorType>
TFlatSet<T, AllocatorType>::TFlatSet(TFlatSet&& other) noexcept
    : Base(std::move(other))
{
}

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::operator=(TFlatSet&& other) noexcept -> TFlatSet&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T, class AllocatorType>
TFlatSet<T, AllocatorType>::~TFlatSet() = default;

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::Find(const T& value) -> Iterator
{
    const auto it = Base::LowerBound(value);

    if (it == End())
    {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::Find(const T& value) const -> ConstIterator
{
    const auto it = Base::LowerBound(value);

    if (it == End())
    {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::Insert(const T& value) -> InsertResult
{
    Iterator it = Base::LowerBound(value);

    if (it == End() || !(*it == value))
    {
        it = Base::Base::Insert(it, value);

        return { it, true };
    }

    return { it, false };
}

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::Insert(T&& value) -> InsertResult
{
    Iterator it = Base::LowerBound(value);

    if (it == End() || !(*it == value))
    {
        it = Base::Base::Insert(it, std::forward<T>(value));

        return { it, true };
    }

    return { it, false };
}

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::Erase(ConstIterator it) -> Iterator
{
    return Base::Erase(it);
}

template <class T, class AllocatorType>
auto TFlatSet<T, AllocatorType>::Erase(const T& value) -> Iterator
{
    const auto it = Find(value);

    if (it == End())
    {
        return End();
    }

    return Base::Erase(it);
}

} // namespace containers

using containers::TFlatSet;

template <class T, class AllocatorType>
struct IsFlatSet<TFlatSet<T, AllocatorType>> : std::true_type
{
};

} // namespace Hyperion
