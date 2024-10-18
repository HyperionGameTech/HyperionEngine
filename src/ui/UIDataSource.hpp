/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_DATA_SOURCE_HPP
#define HYPERION_UI_DATA_SOURCE_HPP

#include <core/Defines.hpp>

#include <core/containers/Forest.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/UUID.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypObject.hpp>

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

    virtual RC<UIObject> CreateUIObject(UIStage *stage, const HypData &value) const = 0;
    virtual RC<UIObject> CreateUIObject(UIStage *stage, const HypData &value, ConstAnyRef context) const = 0;

    virtual void UpdateUIObject(UIObject *ui_object, const HypData &value) const = 0;
    virtual void UpdateUIObject(UIObject *ui_object, const HypData &value, ConstAnyRef context) const = 0;
};

template <class T, class Derived>
class HYP_API UIDataSourceElementFactory : public IUIDataSourceElementFactory
{
public:
    virtual ~UIDataSourceElementFactory() = default;

    virtual RC<UIObject> CreateUIObject(UIStage *stage, const HypData &value) const override final
    {
        return CreateUIObject(stage, value, ConstAnyRef::Empty());
    }

    virtual RC<UIObject> CreateUIObject(UIStage *stage, const HypData &value, ConstAnyRef context) const override final
    {
        Derived factory_instance;
        static_cast<UIDataSourceElementFactory &>(factory_instance).m_context = context;

        RC<UIObject> result;

        if constexpr (std::is_same_v<HypData, T>) {
            return factory_instance.Create(stage, value);
        } else {
            return factory_instance.Create(stage, value.Get<T>());
        }
    }

    virtual void UpdateUIObject(UIObject *ui_object, const HypData &value) const override final
    {
        return UpdateUIObject(ui_object, value, ConstAnyRef::Empty());
    }

    virtual void UpdateUIObject(UIObject *ui_object, const HypData &value, ConstAnyRef context) const override final
    {
        Derived factory_instance;
        static_cast<UIDataSourceElementFactory &>(factory_instance).m_context = context;

        if constexpr (std::is_same_v<HypData, T>) {
            return factory_instance.Update(ui_object, value);
        } else {
            return factory_instance.Update(ui_object, value.Get<T>());
        }
    }

protected:
    template <class ContextType>
    const ContextType *GetContext() const
    {
        return m_context.TryGet<ContextType>();
    }

private:
    ConstAnyRef m_context;
};

class HYP_API UIDataSourceElement
{
public:
    template <class T, typename = std::enable_if_t<!std::is_same_v<T, HypData>>>
    UIDataSourceElement(UUID uuid, T &&value)
        : m_uuid(uuid),
          m_value(HypData(std::forward<T>(value)))
    {
    }

    UIDataSourceElement(UUID uuid, HypData &&value)
        : m_uuid(uuid),
          m_value(std::move(value))
    {
    }

    UIDataSourceElement(const UIDataSourceElement &other)               = delete;
    UIDataSourceElement &operator=(const UIDataSourceElement &other)    = delete;

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

    ~UIDataSourceElement() = default;

    HYP_FORCE_INLINE UUID GetUUID() const
        { return m_uuid; }

    HYP_FORCE_INLINE HypData &GetValue()
        { return m_value; }

    HYP_FORCE_INLINE const HypData &GetValue() const
        { return m_value; }

private:
    UUID    m_uuid;
    HypData m_value;
};

HYP_CLASS(Abstract)
class HYP_API UIDataSourceBase : public EnableRefCountedPtrFromThis<UIDataSourceBase>
{
    HYP_OBJECT_BODY(UIDataSourceBase);

protected:
    UIDataSourceBase(IUIDataSourceElementFactory *element_factory)
        : m_element_factory(element_factory)
    {
        // AssertThrowMsg(element_factory != nullptr, "No element factory registered for the data source; unable to create UIObjects");
    }

public:
    UIDataSourceBase(const UIDataSourceBase &other)                 = delete;
    UIDataSourceBase &operator=(const UIDataSourceBase &other)      = delete;
    UIDataSourceBase(UIDataSourceBase &&other) noexcept             = delete;
    UIDataSourceBase &operator=(UIDataSourceBase &&other) noexcept  = delete;
    virtual ~UIDataSourceBase()                                     = default;

    HYP_FORCE_INLINE IUIDataSourceElementFactory *GetElementFactory() const
        { return m_element_factory; }

    virtual void Push(const UUID &uuid, HypData &&value, const UUID &parent_uuid = UUID::Invalid()) = 0;
    virtual const UIDataSourceElement *Get(const UUID &uuid) const = 0;
    virtual void Set(const UUID &uuid, HypData &&value) = 0;
    virtual bool Remove(const UUID &uuid) = 0;
    virtual void RemoveAllWithPredicate(ProcRef<bool, UIDataSourceElement *> predicate) = 0;

    virtual UIDataSourceElement *FindWithPredicate(ProcRef<bool, const UIDataSourceElement *> predicate) = 0;

    HYP_FORCE_INLINE const UIDataSourceElement *FindWithPredicate(ProcRef<bool, const UIDataSourceElement *> predicate) const
    {
        return const_cast<UIDataSourceBase *>(this)->FindWithPredicate(predicate);
    }

    virtual SizeType Size() const = 0;
    virtual void Clear() = 0;

    virtual Array<Pair<UIDataSourceElement *, UIDataSourceElement *>> GetValues() = 0;

    /*! \brief General purpose delegate that is called whenever the data source changes */
    Delegate<void, UIDataSourceBase *>                                                  OnChange;

    /*! \brief Called when an element is added to the data source */
    Delegate<void, UIDataSourceBase *, UIDataSourceElement *, UIDataSourceElement *>    OnElementAdd;

    /*! \brief Called when an element is removed from the data source */
    Delegate<void, UIDataSourceBase *, UIDataSourceElement *, UIDataSourceElement *>    OnElementRemove;

    /*! \brief Called when an element is updated in the data source */
    Delegate<void, UIDataSourceBase *, UIDataSourceElement *, UIDataSourceElement *>    OnElementUpdate;

protected:
    IUIDataSourceElementFactory                                                         *m_element_factory;
};

HYP_CLASS()
class HYP_API UIDataSource : public UIDataSourceBase
{
    HYP_OBJECT_BODY(UIDataSource);

    static HYP_FORCE_INLINE UIDataSourceElement *GetParentElementFromIterator(typename Forest<UIDataSourceElement>::Iterator iterator)
    {
        return iterator.GetCurrentNode()->GetParent() != nullptr
            ? &**iterator.GetCurrentNode()->GetParent()
            : nullptr;
    }

public:
    // temp : fixme
    UIDataSource()
        : UIDataSource(TypeWrapper<Any> { })
    {
    }

    template <class T>
    UIDataSource(TypeWrapper<T>)
        : UIDataSourceBase(UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(TypeID::ForType<T>())),
          m_element_type_id(TypeID::ForType<T>())
    {
    }

    UIDataSource(const UIDataSource &other)                 = delete;
    UIDataSource &operator=(const UIDataSource &other)      = delete;
    UIDataSource(UIDataSource &&other) noexcept             = delete;
    UIDataSource &operator=(UIDataSource &&other) noexcept  = delete;
    virtual ~UIDataSource() override
    {
        // temp; testing
        DebugLog(LogType::Debug, "Destroy UIDataSource with address %p from C++ side\n", (void*)this);
    }
    
    virtual void Push(const UUID &uuid, HypData &&value, const UUID &parent_uuid) override
    {
        if (!value.IsValid()) {
            return;
        }

        if (value.GetTypeID() != m_element_type_id) {
            HYP_FAIL("Cannot add element with TypeID %u to UIDataSource - expected TypeID %u",
                value.GetTypeID().Value(),
                m_element_type_id.Value());
        }

        // AssertThrowMsg(value.Is<T>(), "Cannot add object not of type %s to data source", TypeName<T>().Data());

        typename Forest<UIDataSourceElement>::ConstIterator parent_it = m_values.End();

        if (parent_uuid != UUID::Invalid()) {
            parent_it = m_values.FindIf([&parent_uuid](const auto &item)
            {
                return item.GetUUID() == parent_uuid;
            });
        }

        auto it = m_values.Add(UIDataSourceElement(uuid, std::move(value)), parent_it);

        OnElementAdd(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);
    }

    virtual const UIDataSourceElement *Get(const UUID &uuid) const override
    {
        auto it = m_values.FindIf([&uuid](const auto &item)
        {
            return item.GetUUID() == uuid;
        });

        if (it == m_values.End()) {
            return nullptr;
        }

        return &*it;
    }

    virtual void Set(const UUID &uuid, HypData &&value) override
    {
        if (value.GetTypeID() != m_element_type_id) {
            HYP_FAIL("Cannot set element with TypeID %u in UIDataSource - expected TypeID %u",
                value.GetTypeID().Value(),
                m_element_type_id.Value());
        }

        auto it = m_values.FindIf([&uuid](const auto &item)
        {
            return item.GetUUID() == uuid;
        });

        if (it == m_values.End()) {
            HYP_FAIL("Element with UUID %s not found", uuid.ToString().Data());
        }

        // AssertThrowMsg(value.Is<T>(), "Cannot add object not of type %s to data source", TypeName<T>().Data());

        *it = UIDataSourceElement(uuid, std::move(value));

        OnElementUpdate(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);

        return;
    }

    virtual bool Remove(const UUID &uuid) override
    {
        bool changed = false;

        for (typename Forest<UIDataSourceElement>::Iterator it = m_values.Begin(); it != m_values.End();) {
            if (it->GetUUID() == uuid) {
                OnElementRemove(this, &*it, GetParentElementFromIterator(it));

                it = m_values.Erase(it);

                changed = true;
            } else {
                ++it;
            }
        }

        if (changed) {
            OnChange(this);
        }

        return changed;
    }

    virtual void RemoveAllWithPredicate(ProcRef<bool, UIDataSourceElement *> predicate) override
    {
        bool changed = false;

        for (typename Forest<UIDataSourceElement>::Iterator it = m_values.Begin(); it != m_values.End();) {
            if (predicate(&*it)) {
                OnElementRemove(this, &*it, GetParentElementFromIterator(it));

                it = m_values.Erase(it);

                changed = true;
            } else {
                ++it;
            }
        }

        if (changed) {
            OnChange(this);
        }
    }

    virtual UIDataSourceElement *FindWithPredicate(ProcRef<bool, const UIDataSourceElement *> predicate) override
    {
        auto it = m_values.FindIf([&predicate](const UIDataSourceElement &item) -> bool
        {
            return predicate(&item);
        });

        if (it != m_values.End()) {
            return &*it;
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
        for (auto it = m_values.Begin(); it != m_values.End(); ++it) {
            OnElementRemove(this, &*it, GetParentElementFromIterator(it));
        }

        m_values.Clear();

        OnChange(this);
    }

    virtual Array<Pair<UIDataSourceElement *, UIDataSourceElement *>> GetValues() override
    {
        Array<Pair<UIDataSourceElement *, UIDataSourceElement *>> values;
        values.Reserve(m_values.Size());

        for (auto it = m_values.Begin(); it != m_values.End(); ++it) {
            values.EmplaceBack(&*it, GetParentElementFromIterator(it));
        }

        return values;
    }

private:
    TypeID                      m_element_type_id;
    Forest<UIDataSourceElement> m_values;
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
} // namespace hyperion

#define HYP_DEFINE_UI_ELEMENT_FACTORY(T, Factory) \
    static ::hyperion::detail::UIDataSourceElementFactoryRegistration<T> HYP_UNIQUE_NAME(UIElementFactory) { new Factory() }

#endif