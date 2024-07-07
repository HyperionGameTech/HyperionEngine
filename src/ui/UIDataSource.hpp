/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_DATA_SOURCE_HPP
#define HYPERION_UI_DATA_SOURCE_HPP

#include <core/Defines.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/UUID.hpp>

#include <core/memory/AnyRef.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/Util.hpp>

namespace hyperion {

class UIObject;
class UIStage;

class IUIDataSourceElementFactory;

class HYP_API UIDataSourceElementFactoryRegistry
{
public:
    static UIDataSourceElementFactoryRegistry &GetInstance();

    IUIDataSourceElementFactory *GetFactory(TypeID type_id) const;
    void RegisterFactory(TypeID type_id, IUIDataSourceElementFactory *element_factory);

private:
    TypeMap<IUIDataSourceElementFactory *>  m_element_factories;
};

class HYP_API IUIDataSourceElementFactory
{
public:
    virtual ~IUIDataSourceElementFactory() = default;

    virtual RC<UIObject> CreateUIObject(UIStage *stage, ConstAnyRef value) const = 0;
};

template <class T>
class HYP_API UIDataSourceElementFactory : public IUIDataSourceElementFactory
{
public:
    virtual ~UIDataSourceElementFactory() = default;

    virtual RC<UIObject> CreateUIObject(UIStage *stage, ConstAnyRef value) const override final
    {
        return CreateUIObject_Internal(stage, value.Get<T>());
    }

protected:
    virtual RC<UIObject> CreateUIObject_Internal(UIStage *stage, const T &value) const = 0;
};

class HYP_API IUIDataSourceElement
{
public:
    virtual ~IUIDataSourceElement() = default;

    virtual UUID GetUUID() const = 0;
    virtual ConstAnyRef GetValue() const = 0;

    template <class T>
    const T &GetValue() const
    {
        ConstAnyRef ref = GetValue();
        AssertThrowMsg(ref.Is<T>(), "Cannot get value of type %u as type %u (%s)",
            ref.GetTypeID().Value(), TypeID::ForType<T>().Value(), TypeName<T>().Data());

        return ref.Get<T>();
    }
};

template <class T>
class HYP_API UIDataSourceElement : public IUIDataSourceElement
{
public:
    UIDataSourceElement(UUID uuid, const T &value)
        : m_uuid(uuid),
          m_value(value)
    {
    }

    UIDataSourceElement(const UIDataSourceElement &other)
        : m_uuid(other.m_uuid),
          m_value(other.m_value)
    {
    }

    UIDataSourceElement &operator=(const UIDataSourceElement &other)
    {
        if (this != &other) {
            m_uuid = other.m_uuid;
            m_value = other.m_value;
        }

        return *this;
    }

    UIDataSourceElement(UIDataSourceElement &&other) noexcept
        : m_uuid(other.m_uuid),
          m_value(std::move(other.m_value))
    {
        other.m_uuid = UUID::Invalid();
    }

    UIDataSourceElement &operator=(UIDataSourceElement &&other) noexcept
    {
        if (this != &other) {
            m_uuid = other.m_uuid;
            m_value = std::move(other.m_value);

            other.m_uuid = UUID::Invalid();
        }

        return *this;
    }

    virtual ~UIDataSourceElement() override = default;

    virtual UUID GetUUID() const override
        { return m_uuid; }

    virtual ConstAnyRef GetValue() const override
        { return ConstAnyRef(&m_value); }

private:
    UUID    m_uuid;
    T       m_value;
};

class HYP_API UIDataSourceBase
{
protected:
    UIDataSourceBase(IUIDataSourceElementFactory *element_factory)
        : m_element_factory(element_factory)
    {
        AssertThrowMsg(element_factory != nullptr, "No element factory registered for the data source; unable to create UIObjects");
    }

public:
    UIDataSourceBase(const UIDataSourceBase &other)                 = delete;
    UIDataSourceBase &operator=(const UIDataSourceBase &other)      = delete;
    UIDataSourceBase(UIDataSourceBase &&other) noexcept             = delete;
    UIDataSourceBase &operator=(UIDataSourceBase &&other) noexcept  = delete;
    virtual ~UIDataSourceBase()                                     = default;

    HYP_FORCE_INLINE
    IUIDataSourceElementFactory *GetElementFactory() const
        { return m_element_factory; }

    virtual void Push(ConstAnyRef value) = 0;
    virtual const IUIDataSourceElement *Get(SizeType index) const = 0;
    virtual const IUIDataSourceElement *Get(const UUID &uuid) const = 0;
    virtual void Set(SizeType index, AnyRef value) = 0;
    virtual void Set(const UUID &uuid, AnyRef value) = 0;
    virtual void Remove(SizeType index) = 0;
    virtual void RemoveAllWithPredicate(const Proc<bool, IUIDataSourceElement *> &predicate) = 0;
    virtual const IUIDataSourceElement *FindWithPredicate(const Proc<bool, const IUIDataSourceElement *> &predicate) const = 0;
    virtual SizeType Size() const = 0;
    virtual void Clear() = 0;

    virtual Array<IUIDataSourceElement *> GetValues() = 0;

    /*! \brief General purpose delegate that is called whenever the data source changes */
    Delegate<void, UIDataSourceBase *>                          OnChange;

    /*! \brief Called when an element is added to the data source */
    Delegate<void, UIDataSourceBase *, IUIDataSourceElement *>  OnElementAdd;

    /*! \brief Called when an element is removed from the data source */
    Delegate<void, UIDataSourceBase *, IUIDataSourceElement *>  OnElementRemove;

    /*! \brief Called when an element is updated in the data source */
    Delegate<void, UIDataSourceBase *, IUIDataSourceElement *>  OnElementUpdate;

protected:
    IUIDataSourceElementFactory                                 *m_element_factory;
};

template <class T>
class HYP_API UIDataSource : public UIDataSourceBase
{
public:
    UIDataSource()
        : UIDataSourceBase(UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(TypeID::ForType<T>()))
    {
    }

    UIDataSource(const UIDataSource &other)                 = delete;
    UIDataSource &operator=(const UIDataSource &other)      = delete;
    UIDataSource(UIDataSource &&other) noexcept             = delete;
    UIDataSource &operator=(UIDataSource &&other) noexcept  = delete;
    virtual ~UIDataSource()                                 = default;
    
    virtual void Push(ConstAnyRef value) override
    {
        if (!value.HasValue()) {
            return;
        }

        AssertThrowMsg(value.Is<T>(), "Cannot object not of type %s to data source", TypeName<T>().Data());

        auto &element = m_values.PushBack(UIDataSourceElement<T>(UUID(), value.Get<T>()));

        OnElementAdd.Broadcast(this, &element);
        OnChange.Broadcast(this);
    }
    
    virtual const IUIDataSourceElement *Get(SizeType index) const override
    {
        AssertThrowMsg(index < m_values.Size(), "Index out of bounds");

        return &m_values[index];
    }

    virtual const IUIDataSourceElement *Get(const UUID &uuid) const override
    {
        // @TODO More efficient lookup
        for (SizeType index = 0; index < m_values.Size(); index++) {
            if (m_values[index].GetUUID() == uuid) {
                return &m_values[index];
            }
        }

        return nullptr;
    }

    virtual void Set(SizeType index, AnyRef value) override
    {
        AssertThrowMsg(index < m_values.Size(), "Index out of bounds");
        AssertThrowMsg(value.Is<T>(), "Cannot object not of type %s to data source", TypeName<T>().Data());

        m_values[index] = UIDataSourceElement<T>(m_values[index].GetUUID(), value.Get<T>());

        OnElementUpdate.Broadcast(this, &m_values[index]);
        OnChange.Broadcast(this);
    }

    virtual void Set(const UUID &uuid, AnyRef value) override
    {
        // @TODO More efficient lookup
        for (SizeType index = 0; index < m_values.Size(); index++) {
            if (m_values[index].GetUUID() == uuid) {
                AssertThrowMsg(value.Is<T>(), "Cannot object not of type %s to data source", TypeName<T>().Data());

                m_values[index] = UIDataSourceElement<T>(m_values[index].GetUUID(), value.Get<T>());

                OnElementUpdate.Broadcast(this, &m_values[index]);
                OnChange.Broadcast(this);

                return;
            }
        }

        HYP_FAIL("Element with UUID %s not found", uuid.ToString().Data());
    }

    virtual void Remove(SizeType index) override
    {
        AssertThrowMsg(index < m_values.Size(), "Index out of bounds");

        OnElementRemove.Broadcast(this, &m_values[index]);

        m_values.EraseAt(index);

        OnChange.Broadcast(this);
    }

    virtual void RemoveAllWithPredicate(const Proc<bool, IUIDataSourceElement *> &predicate) override
    {
        Array<IUIDataSourceElement *> elements_to_remove;

        for (SizeType index = 0; index < m_values.Size(); index++) {
            if (predicate(&m_values[index])) {
                elements_to_remove.PushBack(&m_values[index]);
            }
        }

        if (elements_to_remove.Any()) {
            for (SizeType index = 0; index < elements_to_remove.Size(); index++) {
                OnElementRemove.Broadcast(this, elements_to_remove[index]);

                const auto it = m_values.FindIf([element_to_remove = elements_to_remove[index]](const UIDataSourceElement<T> &element)
                {
                    return &element == element_to_remove;
                });

                if (it != m_values.End()) {
                    m_values.Erase(it);
                }
            }

            OnChange.Broadcast(this);
        }
    }

    virtual const IUIDataSourceElement *FindWithPredicate(const Proc<bool, const IUIDataSourceElement *> &predicate) const override
    {
        for (SizeType index = 0; index < m_values.Size(); index++) {
            if (predicate(&m_values[index])) {
                return &m_values[index];
            }
        }

        return nullptr;
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
            OnElementRemove.Broadcast(this, &m_values[index]);
        }

        m_values.Clear();

        OnChange.Broadcast(this);
    }

    virtual Array<IUIDataSourceElement *> GetValues() override
    {
        Array<IUIDataSourceElement *> values;
        values.Resize(m_values.Size());

        for (SizeType index = 0; index < m_values.Size(); index++) {
            values[index] = &m_values[index];
        }

        return values;
    }

private:
    Array<UIDataSourceElement<T>>   m_values;
};

namespace detail {

struct HYP_API UIDataSourceElementFactoryRegistrationBase
{
protected:
    IUIDataSourceElementFactory *element_factory;

    UIDataSourceElementFactoryRegistrationBase(TypeID type_id, IUIDataSourceElementFactory *element_factory);
    ~UIDataSourceElementFactoryRegistrationBase();
};

template <class T>
struct UIDataSourceElementFactoryRegistration : public UIDataSourceElementFactoryRegistrationBase
{
    UIDataSourceElementFactoryRegistration(IUIDataSourceElementFactory *element_factory)
        : UIDataSourceElementFactoryRegistrationBase(TypeID::ForType<T>(), element_factory)
    {
    }
};

} // namespace detail

#define HYP_DEFINE_UI_ELEMENT_FACTORY(T, Factory) \
    static ::hyperion::detail::UIDataSourceElementFactoryRegistration<T> HYP_UNIQUE_NAME(UIElementFactory) { new Factory() }

} // namespace hyperion

#endif