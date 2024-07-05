/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_DATA_SOURCE_HPP
#define HYPERION_UI_DATA_SOURCE_HPP

#include <core/Defines.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/utilities/Variant.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/UUID.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/Util.hpp>

namespace hyperion {

class HYP_API UIDataSourceBase
{
public:
    UIDataSourceBase()                                              = default;
    UIDataSourceBase(const UIDataSourceBase &other)                 = delete;
    UIDataSourceBase &operator=(const UIDataSourceBase &other)      = delete;
    UIDataSourceBase(UIDataSourceBase &&other) noexcept             = delete;
    UIDataSourceBase &operator=(UIDataSourceBase &&other) noexcept  = delete;
    virtual ~UIDataSourceBase()                                     = default;

    virtual void Push(ConstAnyRef value) = 0;
    virtual ConstAnyRef Get(SizeType index) const = 0;
    virtual ConstAnyRef Get(const UUID &uuid) const = 0;
    virtual void Set(SizeType index, AnyRef value) = 0;
    virtual void Remove(SizeType index) = 0;
    virtual void RemoveAllWithPredicate(const Proc<bool, UUID, ConstAnyRef> &predicate) = 0;
    virtual ConstAnyRef FindWithPredicate(const Proc<bool, UUID, ConstAnyRef> &predicate) const = 0;
    virtual SizeType Size() const = 0;
    virtual void Clear() = 0;

    virtual Array<Pair<UUID, AnyRef>> GetValues() = 0;

    /*! \brief General purpose delegate that is called whenever the data source changes */
    Delegate<void, UIDataSourceBase *>                      OnChange;

    /*! \brief Called when an element is added to the data source */
    Delegate<void, UIDataSourceBase *, UUID, ConstAnyRef>   OnElementAdd;

    /*! \brief Called when an element is removed from the data source */
    Delegate<void, UIDataSourceBase *, UUID, ConstAnyRef>   OnElementRemove;

    /*! \brief Called when an element is updated in the data source */
    Delegate<void, UIDataSourceBase *, UUID, ConstAnyRef>   OnElementUpdate;
};

template <class T>
class HYP_API UIDataSource : public UIDataSourceBase
{
public:
    UIDataSource()                                          = default;
    UIDataSource(const UIDataSource &other)                 = delete;
    UIDataSource &operator=(const UIDataSource &other)      = delete;
    UIDataSource(UIDataSource &&other) noexcept             = delete;
    UIDataSource &operator=(UIDataSource &&other) noexcept  = delete;
    virtual ~UIDataSource()                                 = default;

    HYP_FORCE_INLINE
    const Array<Pair<UUID, T>> &GetArray() const
        { return m_values; }
    
    virtual void Push(ConstAnyRef value) override
    {
        if (!value.HasValue()) {
            return;
        }

        AssertThrowMsg(value.Is<T>(), "Cannot object not of type %s to data source", TypeName<T>().Data());

        auto &element = m_values.PushBack({ UUID(), value.Get<T>() });

        OnElementAdd.Broadcast(this, element.first, ConstAnyRef(&element.second));
        OnChange.Broadcast(this);
    }
    
    virtual ConstAnyRef Get(SizeType index) const override
    {
        AssertThrowMsg(index < m_values.Size(), "Index out of bounds");

        return ConstAnyRef(m_values[index].second);
    }

    virtual ConstAnyRef Get(const UUID &uuid) const override
    {
        // @TODO More efficient lookup
        for (SizeType index = 0; index < m_values.Size(); index++) {
            if (m_values[index].first == uuid) {
                return ConstAnyRef(&m_values[index].second);
            }
        }

        return ConstAnyRef::Empty();
    }

    virtual void Set(SizeType index, AnyRef value) override
    {
        AssertThrowMsg(index < m_values.Size(), "Index out of bounds");
        AssertThrowMsg(value.Is<T>(), "Cannot object not of type %s to data source", TypeName<T>().Data());

        m_values[index] = { UUID(), value.Get<T>() };

        OnElementUpdate.Broadcast(this, m_values[index].first, ConstAnyRef(&m_values[index].second));
        OnChange.Broadcast(this);
    }

    virtual void Remove(SizeType index) override
    {
        AssertThrowMsg(index < m_values.Size(), "Index out of bounds");

        OnElementRemove.Broadcast(this, m_values[index].first, ConstAnyRef(&m_values[index].second));

        m_values.EraseAt(index);

        OnChange.Broadcast(this);
    }

    virtual void RemoveAllWithPredicate(const Proc<bool, UUID, ConstAnyRef> &predicate) override
    {
        Array<SizeType> indices_to_remove;

        for (SizeType index = 0; index < m_values.Size(); index++) {
            if (predicate(m_values[index].first, m_values[index].second)) {
                indices_to_remove.PushBack(index);
            }
        }

        if (indices_to_remove.Any()) {
            for (SizeType index = 0; index < indices_to_remove.Size(); index++) {
                OnElementRemove.Broadcast(this, m_values[indices_to_remove[index]].first, ConstAnyRef(&m_values[indices_to_remove[index]].second));

                m_values.EraseAt(indices_to_remove[index]);
            }

            OnChange.Broadcast(this);
        }
    }

    virtual ConstAnyRef FindWithPredicate(const Proc<bool, UUID, ConstAnyRef> &predicate) const override
    {
        for (SizeType index = 0; index < m_values.Size(); index++) {
            if (predicate(m_values[index].first, m_values[index].second)) {
                return ConstAnyRef(&m_values[index].second);
            }
        }

        return ConstAnyRef::Empty();
    }

    virtual SizeType Size() const override
    {
        return m_values.Size();
    }

    virtual void Clear() override
    {
        // @TODO check if delegate has any functions bound,
        // or better yet, have a way to call in bulk (prevent lots of mutex locks/unlocks)
        for (SizeType index = 0; index < m_values.Size(); index++) {
            OnElementRemove.Broadcast(this, m_values[index].first, ConstAnyRef(&m_values[index].second));
        }

        m_values.Clear();

        OnChange.Broadcast(this);
    }

    virtual Array<Pair<UUID, AnyRef>> GetValues() override
    {
        Array<Pair<UUID, AnyRef>> refs;
        refs.Resize(m_values.Size());

        for (SizeType index = 0; index < m_values.Size(); index++) {
            refs[index] = Pair<UUID, AnyRef> { m_values[index].first, AnyRef(&m_values[index].second) };
        }

        return refs;
    }

private:
    Array<Pair<UUID, T>>    m_values;
};

} // namespace hyperion

#endif