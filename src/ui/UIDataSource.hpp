/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_DATA_SOURCE_HPP
#define HYPERION_UI_DATA_SOURCE_HPP

#include <core/Defines.hpp>

#include <core/containers/Forest.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/UUID.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypObject.hpp>

#include <core/Util.hpp>

namespace hyperion {

class World;
class UIObject;
class UIStage;
class World;

class UIElementFactoryBase;

class HYP_API UIElementFactoryRegistry
{
    struct FactoryInstance
    {
        RC<UIElementFactoryBase> (*make_factory_function)(void);
        RC<UIElementFactoryBase> factory_instance;
    };

public:
    static UIElementFactoryRegistry& GetInstance();

    UIElementFactoryBase* GetFactory(TypeID type_id);
    void RegisterFactory(TypeID type_id, RC<UIElementFactoryBase> (*make_factory_function)(void));

private:
    TypeMap<FactoryInstance> m_element_factories;
};

HYP_CLASS(Abstract)
class HYP_API UIElementFactoryBase : public EnableRefCountedPtrFromThis<UIElementFactoryBase>
{
    HYP_OBJECT_BODY(UIElementFactoryBase);

public:
    UIElementFactoryBase() = default;
    virtual ~UIElementFactoryBase() = default;

    HYP_METHOD(Scriptable)
    Handle<UIObject> CreateUIObject(UIObject* parent, const HypData& value, const HypData& context) const;

    HYP_METHOD(Scriptable)
    void UpdateUIObject(UIObject* ui_object, const HypData& value, const HypData& context) const;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* parent, const HypData& value, const HypData& context) const
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void UpdateUIObject_Impl(UIObject* ui_object, const HypData& value, const HypData& context) const
    {
        HYP_PURE_VIRTUAL();
    }
};

template <class T, class Derived>
class HYP_API UIElementFactory : public UIElementFactoryBase
{
public:
    virtual ~UIElementFactory() = default;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* parent, const HypData& value, const HypData& context) const override final
    {
        HYP_MT_CHECK_RW(m_context_data_race_detector);

        m_context = context.ToRef();
        HYP_DEFER({ m_context = AnyRef(); });

        Handle<UIObject> result;

        if constexpr (is_hypdata_v<T>)
        {
            return GetDerived().Create(parent, value);
        }
        else
        {
            return GetDerived().Create(parent, value.Get<T>());
        }
    }

    virtual void UpdateUIObject_Impl(UIObject* ui_object, const HypData& value, const HypData& context) const override final
    {
        HYP_MT_CHECK_RW(m_context_data_race_detector);

        m_context = context.ToRef();
        HYP_DEFER({ m_context = AnyRef(); });

        if constexpr (is_hypdata_v<T>)
        {
            return GetDerived().Update(ui_object, value);
        }
        else
        {
            return GetDerived().Update(ui_object, value.Get<T>());
        }
    }

    template <class ContextType>
    const ContextType* GetContext() const
    {
        HYP_MT_CHECK_READ(m_context_data_race_detector);

        return m_context.TryGet<ContextType>();
    }

private:
    HYP_FORCE_INLINE Derived& GetDerived()
    {
        return static_cast<Derived&>(*this);
    }

    HYP_FORCE_INLINE const Derived& GetDerived() const
    {
        return static_cast<const Derived&>(*this);
    }

    mutable AnyRef m_context;
    DataRaceDetector m_context_data_race_detector;
};

class HYP_API UIDataSourceElement
{
public:
    template <class T, typename = std::enable_if_t<!is_hypdata_v<T>>>
    UIDataSourceElement(UUID uuid, T&& value)
        : m_uuid(uuid),
          m_value(HypData(std::forward<T>(value)))
    {
    }

    UIDataSourceElement(UUID uuid, HypData&& value)
        : m_uuid(uuid),
          m_value(std::move(value))
    {
    }

    UIDataSourceElement(const UIDataSourceElement& other) = delete;
    UIDataSourceElement& operator=(const UIDataSourceElement& other) = delete;

    UIDataSourceElement(UIDataSourceElement&& other) noexcept
        : m_uuid(other.m_uuid),
          m_value(std::move(other.m_value))
    {
        other.m_uuid = UUID::Invalid();
    }

    UIDataSourceElement& operator=(UIDataSourceElement&& other) noexcept
    {
        if (this != &other)
        {
            m_uuid = other.m_uuid;
            m_value = std::move(other.m_value);

            other.m_uuid = UUID::Invalid();
        }

        return *this;
    }

    ~UIDataSourceElement() = default;

    HYP_FORCE_INLINE UUID GetUUID() const
    {
        return m_uuid;
    }

    HYP_FORCE_INLINE HypData& GetValue()
    {
        return m_value;
    }

    HYP_FORCE_INLINE const HypData& GetValue() const
    {
        return m_value;
    }

private:
    UUID m_uuid;
    HypData m_value;
};

HYP_CLASS(Abstract)
class HYP_API UIDataSourceBase : public HypObject<UIDataSourceBase>
{
    HYP_OBJECT_BODY(UIDataSourceBase);

protected:
    UIDataSourceBase(UIElementFactoryBase* element_factory)
        : m_element_factory(element_factory)
    {
        // AssertThrowMsg(element_factory != nullptr, "No element factory registered for the data source; unable to create UIObjects");
    }

public:
    UIDataSourceBase(const UIDataSourceBase& other) = delete;
    UIDataSourceBase& operator=(const UIDataSourceBase& other) = delete;
    UIDataSourceBase(UIDataSourceBase&& other) noexcept = delete;
    UIDataSourceBase& operator=(UIDataSourceBase&& other) noexcept = delete;
    virtual ~UIDataSourceBase() = default;

    HYP_FORCE_INLINE UIElementFactoryBase* GetElementFactory() const
    {
        return m_element_factory;
    }

    virtual void Push(const UUID& uuid, HypData&& value, const UUID& parent_uuid = UUID::Invalid()) = 0;
    virtual const UIDataSourceElement* Get(const UUID& uuid) const = 0;
    virtual void Set(const UUID& uuid, HypData&& value) = 0;
    virtual void ForceUpdate(const UUID& uuid) = 0;
    virtual bool Remove(const UUID& uuid) = 0;
    virtual void RemoveAllWithPredicate(ProcRef<bool(UIDataSourceElement*)> predicate) = 0;

    virtual Handle<UIObject> CreateUIObject(UIObject* parent, const HypData& value, const HypData& context) const = 0;
    virtual void UpdateUIObject(UIObject* ui_object, const HypData& value, const HypData& context) const = 0;

    virtual UIDataSourceElement* FindWithPredicate(ProcRef<bool(const UIDataSourceElement*)> predicate) = 0;

    HYP_FORCE_INLINE const UIDataSourceElement* FindWithPredicate(ProcRef<bool(const UIDataSourceElement*)> predicate) const
    {
        return const_cast<UIDataSourceBase*>(this)->FindWithPredicate(predicate);
    }

    HYP_METHOD()
    virtual int Size() const = 0;

    HYP_METHOD()
    virtual void Clear() = 0;

    virtual Array<Pair<UIDataSourceElement*, UIDataSourceElement*>> GetValues() = 0;

    /*! \brief General purpose delegate that is called whenever the data source changes */
    Delegate<void, UIDataSourceBase*> OnChange;

    /*! \brief Called when an element is added to the data source */
    Delegate<void, UIDataSourceBase*, UIDataSourceElement*, UIDataSourceElement*> OnElementAdd;

    /*! \brief Called when an element is removed from the data source */
    Delegate<void, UIDataSourceBase*, UIDataSourceElement*, UIDataSourceElement*> OnElementRemove;

    /*! \brief Called when an element is updated in the data source */
    Delegate<void, UIDataSourceBase*, UIDataSourceElement*, UIDataSourceElement*> OnElementUpdate;

protected:
    UIElementFactoryBase* m_element_factory;
};

HYP_CLASS()
class HYP_API UIDataSource : public UIDataSourceBase
{
    HYP_OBJECT_BODY(UIDataSource);

    static HYP_FORCE_INLINE UIDataSourceElement* GetParentElementFromIterator(typename Forest<UIDataSourceElement>::Iterator iterator)
    {
        return iterator.GetCurrentNode()->GetParent() != nullptr
            ? &**iterator.GetCurrentNode()->GetParent()
            : nullptr;
    }

public:
    // temp : required for HypClass
    UIDataSource()
        : UIDataSource(TypeWrapper<Any> {})
    {
    }

    template <class T>
    UIDataSource(TypeWrapper<T>)
        : UIDataSourceBase(UIElementFactoryRegistry::GetInstance().GetFactory(TypeID::ForType<T>())),
          m_element_type_id(TypeID::ForType<T>())
    {
    }

    template <class T, class CreateUIObjectFunction, class UpdateUIObjectFunction>
    UIDataSource(TypeWrapper<T>, CreateUIObjectFunction&& create_ui_object, UpdateUIObjectFunction&& update_ui_object)
        : UIDataSourceBase(UIElementFactoryRegistry::GetInstance().GetFactory(TypeID::ForType<T>())),
          m_element_type_id(TypeID::ForType<T>()),
          m_create_ui_object_proc([func = std::forward<CreateUIObjectFunction>(create_ui_object)](UIObject* parent, const HypData& value, const HypData& context) mutable
              {
                  return func(parent, value.Get<T>(), context);
              }),
          m_update_ui_object_proc([func = std::forward<UpdateUIObjectFunction>(update_ui_object)](UIObject* ui_object, const HypData& value, const HypData& context) mutable
              {
                  func(ui_object, value.Get<T>(), context);
              })
    {
    }

    UIDataSource(const UIDataSource& other) = delete;
    UIDataSource& operator=(const UIDataSource& other) = delete;
    UIDataSource(UIDataSource&& other) noexcept = delete;
    UIDataSource& operator=(UIDataSource&& other) noexcept = delete;
    virtual ~UIDataSource() override = default;

    virtual void Push(const UUID& uuid, HypData&& value, const UUID& parent_uuid) override
    {
        if (value.IsNull())
        {
            return;
        }

        if (value.GetTypeID() != m_element_type_id)
        {
            HYP_FAIL("Cannot add element with TypeID %u to UIDataSource - expected TypeID %u",
                value.GetTypeID().Value(),
                m_element_type_id.Value());
        }

        // AssertThrowMsg(value.Is<T>(), "Cannot add object not of type %s to data source", TypeName<T>().Data())
        
        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it != m_values.End())
        {
            HYP_FAIL("Element with UUID %s already exists in the data source", uuid.ToString().Data());
        }

        typename Forest<UIDataSourceElement>::ConstIterator parent_it = m_values.End();

        if (parent_uuid != UUID::Invalid())
        {
            parent_it = m_values.FindIf([&parent_uuid](const auto& item)
                {
                    return item.GetUUID() == parent_uuid;
                });
        }

        it = m_values.Add(UIDataSourceElement(uuid, std::move(value)), parent_it);

        OnElementAdd(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);
    }

    virtual const UIDataSourceElement* Get(const UUID& uuid) const override
    {
        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it == m_values.End())
        {
            return nullptr;
        }

        return &*it;
    }

    virtual void Set(const UUID& uuid, HypData&& value) override
    {
        if (value.GetTypeID() != m_element_type_id)
        {
            HYP_FAIL("Cannot set element with TypeID %u in UIDataSource - expected TypeID %u",
                value.GetTypeID().Value(),
                m_element_type_id.Value());
        }

        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it == m_values.End())
        {
            HYP_FAIL("Element with UUID %s not found", uuid.ToString().Data());
        }

        // AssertThrowMsg(value.Is<T>(), "Cannot add object not of type %s to data source", TypeName<T>().Data());

        *it = UIDataSourceElement(uuid, std::move(value));

        OnElementUpdate(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);
    }

    virtual void ForceUpdate(const UUID& uuid) override
    {
        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it == m_values.End())
        {
            HYP_FAIL("Element with UUID %s not found", uuid.ToString().Data());
        }

        OnElementUpdate(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);
    }

    virtual bool Remove(const UUID& uuid) override
    {
        bool changed = false;

        for (typename Forest<UIDataSourceElement>::Iterator it = m_values.Begin(); it != m_values.End();)
        {
            if (it->GetUUID() == uuid)
            {
                OnElementRemove(this, &*it, GetParentElementFromIterator(it));

                it = m_values.Erase(it);

                changed = true;
            }
            else
            {
                ++it;
            }
        }

        if (changed)
        {
            OnChange(this);
        }

        return changed;
    }

    virtual void RemoveAllWithPredicate(ProcRef<bool(UIDataSourceElement*)> predicate) override
    {
        bool changed = false;

        for (typename Forest<UIDataSourceElement>::Iterator it = m_values.Begin(); it != m_values.End();)
        {
            if (predicate(&*it))
            {
                OnElementRemove(this, &*it, GetParentElementFromIterator(it));

                it = m_values.Erase(it);

                changed = true;
            }
            else
            {
                ++it;
            }
        }

        if (changed)
        {
            OnChange(this);
        }
    }

    virtual UIDataSourceElement* FindWithPredicate(ProcRef<bool(const UIDataSourceElement*)> predicate) override
    {
        auto it = m_values.FindIf([&predicate](const UIDataSourceElement& item) -> bool
            {
                return predicate(&item);
            });

        if (it != m_values.End())
        {
            return &*it;
        }

        return nullptr;
    }

    virtual Handle<UIObject> CreateUIObject(UIObject* parent, const HypData& value, const HypData& context) const override
    {
        if (m_create_ui_object_proc.IsValid())
        {
            return m_create_ui_object_proc(parent, value, context);
        }

        if (m_element_factory)
        {
            return m_element_factory->CreateUIObject(parent, value, context);
        }
        else
        {
            HYP_FAIL("No element factory registered for the data source; unable to create UIObjects");
        }

        return nullptr;
    }

    virtual void UpdateUIObject(UIObject* ui_object, const HypData& value, const HypData& context) const override
    {
        if (m_update_ui_object_proc.IsValid())
        {
            m_update_ui_object_proc(ui_object, value, context);
            return;
        }

        if (m_element_factory)
        {
            m_element_factory->UpdateUIObject(ui_object, value, context);
        }
        else
        {
            HYP_FAIL("No element factory registered for the data source; unable to create UIObjects");
        }
    }

    HYP_METHOD()
    virtual int Size() const override
    {
        return int(m_values.Size());
    }

    HYP_METHOD()
    virtual void Clear() override
    {
        // @TODO check if delegate has any functions bound,
        // or better yet, have a way to call in bulk (prevent lots of mutex locks/unlocks)
        for (auto it = m_values.Begin(); it != m_values.End(); ++it)
        {
            OnElementRemove(this, &*it, GetParentElementFromIterator(it));
        }

        m_values.Clear();

        OnChange(this);
    }

    virtual Array<Pair<UIDataSourceElement*, UIDataSourceElement*>> GetValues() override
    {
        Array<Pair<UIDataSourceElement*, UIDataSourceElement*>> values;
        values.Reserve(m_values.Size());

        for (auto it = m_values.Begin(); it != m_values.End(); ++it)
        {
            values.EmplaceBack(&*it, GetParentElementFromIterator(it));
        }

        return values;
    }

    /*! \internal */
    void SetElementTypeIDAndFactory(TypeID element_type_id, UIElementFactoryBase* element_factory)
    {
        m_element_factory = element_factory;
        m_element_type_id = element_type_id;
    }

private:
    TypeID m_element_type_id;
    Forest<UIDataSourceElement> m_values;

    Proc<Handle<UIObject>(UIObject*, const HypData&, const HypData&)> m_create_ui_object_proc;
    Proc<void(UIObject*, const HypData&, const HypData&)> m_update_ui_object_proc;
};

struct HYP_API UIElementFactoryRegistrationBase
{
protected:
    RC<UIElementFactoryBase> (*m_make_factory_function)(void);

    UIElementFactoryRegistrationBase(TypeID type_id, RC<UIElementFactoryBase> (*make_factory_function)(void));
    ~UIElementFactoryRegistrationBase();
};

template <class T>
struct UIElementFactoryRegistration : public UIElementFactoryRegistrationBase
{
    UIElementFactoryRegistration(RC<UIElementFactoryBase> (*make_factory_function)(void))
        : UIElementFactoryRegistrationBase(TypeID::ForType<T>(), make_factory_function)
    {
    }
};

} // namespace hyperion

#define HYP_DEFINE_UI_ELEMENT_FACTORY(T, Factory)                                                \
    static ::hyperion::UIElementFactoryRegistration<T> HYP_UNIQUE_NAME(UIElementFactory) \
    {                                                                                            \
        []() -> RC<UIElementFactoryBase> {                                                       \
            return MakeRefCountedPtr<Factory>();                                                 \
        }                                                                                        \
    }

#endif