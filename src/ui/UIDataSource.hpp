/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/Forest.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Uuid.hpp>
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
        Handle<UIElementFactoryBase> (*makeFactoryFunction)(void);
        Handle<UIElementFactoryBase> factoryInstance;
    };

public:
    static UIElementFactoryRegistry& GetInstance();

    UIElementFactoryBase* GetFactory(TypeId typeId);
    void RegisterFactory(TypeId typeId, Handle<UIElementFactoryBase> (*makeFactoryFunction)(void));

private:
    TypeMap<FactoryInstance> m_elementFactories;
};

HYP_CLASS(Abstract)
class HYP_API UIElementFactoryBase : public HypObjectBase
{
    HYP_OBJECT_BODY(UIElementFactoryBase);

public:
    UIElementFactoryBase() = default;
    virtual ~UIElementFactoryBase() = default;

    HYP_METHOD(Scriptable)
    Handle<UIObject> CreateUIObject(UIObject* parent, const HypData& value, const HypData& context) const;

    HYP_METHOD(Scriptable)
    void UpdateUIObject(UIObject* uiObject, const HypData& value, const HypData& context) const;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* parent, const HypData& value, const HypData& context) const
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void UpdateUIObject_Impl(UIObject* uiObject, const HypData& value, const HypData& context) const
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
        HYP_MT_CHECK_RW(m_contextDataRaceDetector);

        m_context = context.ToRef();
        HYP_DEFER({ m_context = AnyRef(); });

        Handle<UIObject> result;

        if constexpr (isHypData<T>)
        {
            return GetDerived().Create(parent, value);
        }
        else
        {
            return GetDerived().Create(parent, value.Get<T>());
        }
    }

    virtual void UpdateUIObject_Impl(UIObject* uiObject, const HypData& value, const HypData& context) const override final
    {
        HYP_MT_CHECK_RW(m_contextDataRaceDetector);

        m_context = context.ToRef();
        HYP_DEFER({ m_context = AnyRef(); });

        if constexpr (isHypData<T>)
        {
            return GetDerived().Update(uiObject, value);
        }
        else
        {
            return GetDerived().Update(uiObject, value.Get<T>());
        }
    }

    template <class ContextType>
    const ContextType* GetContext() const
    {
        HYP_MT_CHECK_READ(m_contextDataRaceDetector);

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
    DataRaceDetector m_contextDataRaceDetector;
};

class HYP_API UIDataSourceElement
{
public:
    template <class T, typename = std::enable_if_t<!isHypData<T>>>
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
class HYP_API UIDataSourceBase : public HypObjectBase
{
    HYP_OBJECT_BODY(UIDataSourceBase);

protected:
    UIDataSourceBase(UIElementFactoryBase* elementFactory)
        : m_elementFactory(elementFactory)
    {
        // Assert(elementFactory != nullptr, "No element factory registered for the data source; unable to create UIObjects");
    }

public:
    UIDataSourceBase(const UIDataSourceBase& other) = delete;
    UIDataSourceBase& operator=(const UIDataSourceBase& other) = delete;
    UIDataSourceBase(UIDataSourceBase&& other) noexcept = delete;
    UIDataSourceBase& operator=(UIDataSourceBase&& other) noexcept = delete;
    virtual ~UIDataSourceBase() = default;

    HYP_FORCE_INLINE UIElementFactoryBase* GetElementFactory() const
    {
        return m_elementFactory;
    }

    virtual void Push(const UUID& uuid, HypData&& value, const UUID& parentUuid = UUID::Invalid()) = 0;
    virtual const UIDataSourceElement* Get(const UUID& uuid) const = 0;
    virtual void Set(const UUID& uuid, HypData&& value) = 0;
    virtual void ForceUpdate(const UUID& uuid) = 0;
    virtual bool Remove(const UUID& uuid) = 0;
    virtual void RemoveAllWithPredicate(ProcRef<bool(UIDataSourceElement*)> predicate) = 0;

    virtual Handle<UIObject> CreateUIObject(UIObject* parent, const HypData& value, const HypData& context) const = 0;
    virtual void UpdateUIObject(UIObject* uiObject, const HypData& value, const HypData& context) const = 0;

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
    UIElementFactoryBase* m_elementFactory;
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
        : UIDataSourceBase(UIElementFactoryRegistry::GetInstance().GetFactory(TypeId::ForType<T>())),
          m_elementTypeId(TypeId::ForType<T>())
    {
    }

    template <class T, class CreateUIObjectFunction, class UpdateUIObjectFunction>
    UIDataSource(TypeWrapper<T>, CreateUIObjectFunction&& createUiObject, UpdateUIObjectFunction&& updateUiObject)
        : UIDataSourceBase(UIElementFactoryRegistry::GetInstance().GetFactory(TypeId::ForType<T>())),
          m_elementTypeId(TypeId::ForType<T>()),
          m_createUiObjectProc([func = std::forward<CreateUIObjectFunction>(createUiObject)](UIObject* parent, const HypData& value, const HypData& context) mutable
              {
                  return func(parent, value.Get<T>(), context);
              }),
          m_updateUiObjectProc([func = std::forward<UpdateUIObjectFunction>(updateUiObject)](UIObject* uiObject, const HypData& value, const HypData& context) mutable
              {
                  func(uiObject, value.Get<T>(), context);
              })
    {
    }

    UIDataSource(const UIDataSource& other) = delete;
    UIDataSource& operator=(const UIDataSource& other) = delete;
    UIDataSource(UIDataSource&& other) noexcept = delete;
    UIDataSource& operator=(UIDataSource&& other) noexcept = delete;
    virtual ~UIDataSource() override = default;

    virtual void Push(const UUID& uuid, HypData&& value, const UUID& parentUuid) override
    {
        if (value.IsNull())
        {
            return;
        }

        if (value.GetTypeId() != m_elementTypeId && !hyperion::IsA(GetClass(m_elementTypeId), GetClass(value.GetTypeId())))
        {
            HYP_FAIL("Cannot add element with TypeId %u to UIDataSource - expected TypeId %u",
                value.GetTypeId().Value(),
                m_elementTypeId.Value());
        }

        // Assert(value.Is<T>(), "Cannot add object not of type %s to data source", TypeName<T>().Data())

        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it != m_values.End())
        {
            HYP_FAIL("Element with UUID %s already exists in the data source", uuid.ToString().Data());
        }

        typename Forest<UIDataSourceElement>::ConstIterator parentIt = m_values.End();

        if (parentUuid != UUID::Invalid())
        {
            parentIt = m_values.FindIf([&parentUuid](const auto& item)
                {
                    return item.GetUUID() == parentUuid;
                });
        }

        it = m_values.Add(UIDataSourceElement(uuid, std::move(value)), parentIt);

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
        if (value.GetTypeId() != m_elementTypeId && !hyperion::IsA(GetClass(m_elementTypeId), GetClass(value.GetTypeId())))
        {
            HYP_FAIL("Cannot set element with TypeId %u in UIDataSource - expected TypeId %u",
                value.GetTypeId().Value(),
                m_elementTypeId.Value());
        }

        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it == m_values.End())
        {
            HYP_FAIL("Element with UUID %s not found", uuid.ToString().Data());
        }

        // Assert(value.Is<T>(), "Cannot add object not of type %s to data source", TypeName<T>().Data());

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
        if (m_createUiObjectProc.IsValid())
        {
            return m_createUiObjectProc(parent, value, context);
        }

        if (m_elementFactory)
        {
            return m_elementFactory->CreateUIObject(parent, value, context);
        }
        else
        {
            HYP_FAIL("No element factory registered for the data source; unable to create UIObjects");
        }

        return nullptr;
    }

    virtual void UpdateUIObject(UIObject* uiObject, const HypData& value, const HypData& context) const override
    {
        if (m_updateUiObjectProc.IsValid())
        {
            m_updateUiObjectProc(uiObject, value, context);
            return;
        }

        if (m_elementFactory)
        {
            m_elementFactory->UpdateUIObject(uiObject, value, context);
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
    void SetElementTypeIdAndFactory(TypeId elementTypeId, UIElementFactoryBase* elementFactory)
    {
        m_elementFactory = elementFactory;
        m_elementTypeId = elementTypeId;
    }

private:
    TypeId m_elementTypeId;
    Forest<UIDataSourceElement> m_values;

    Proc<Handle<UIObject>(UIObject*, const HypData&, const HypData&)> m_createUiObjectProc;
    Proc<void(UIObject*, const HypData&, const HypData&)> m_updateUiObjectProc;
};

struct HYP_API UIElementFactoryRegistrationBase
{
protected:
    Handle<UIElementFactoryBase> (*m_makeFactoryFunction)(void);

    UIElementFactoryRegistrationBase(TypeId typeId, Handle<UIElementFactoryBase> (*makeFactoryFunction)(void));
    ~UIElementFactoryRegistrationBase();
};

template <class T>
struct UIElementFactoryRegistration : public UIElementFactoryRegistrationBase
{
    UIElementFactoryRegistration(Handle<UIElementFactoryBase> (*makeFactoryFunction)(void))
        : UIElementFactoryRegistrationBase(TypeId::ForType<T>(), makeFactoryFunction)
    {
    }
};

} // namespace hyperion

#define HYP_DEFINE_UI_ELEMENT_FACTORY(T, Factory)                                                \
    static ::hyperion::UIElementFactoryRegistration<T> HYP_UNIQUE_NAME(UIElementFactory) \
    {                                                                                            \
        []() -> Handle<UIElementFactoryBase> {                                                       \
            return CreateObject<Factory>();                                                 \
        }                                                                                        \
    }

