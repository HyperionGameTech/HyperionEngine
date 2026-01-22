/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Util.hpp>

#include <core/containers/Forest.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/reflection/TypeInfoFwd.hpp>
#include <core/reflection/BoxedValue.hpp>
#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

namespace Hyperion {

class World;
class UIObject;
class UIStage;
class World;

class UIElementFactoryBase;

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

class HYP_API UIElementFactoryRegistry
{
    struct FactoryInstance
    {
        Handle<UIElementFactoryBase> (*makeFactoryFunction)(void) = nullptr;
        Handle<UIElementFactoryBase> factoryInstance = nullptr;
    };

public:
    static UIElementFactoryRegistry& GetInstance();

    Handle<UIElementFactoryBase> GetFactory(const TypeInfo& typeInfo);

    template <class... Types>
    Array<Handle<UIElementFactoryBase>> GetFactories()
    {
        Array<Handle<UIElementFactoryBase>> factories;
        factories.Reserve(sizeof...(Types));

        const FixedArray<const TypeInfo*, sizeof...(Types)> typeInfos = { &TypeOf<Types>()... };

        for (const TypeInfo* typeInfo : typeInfos)
        {
            Handle<UIElementFactoryBase> factory = GetFactory(*typeInfo);
            Assert(factory != nullptr);

            factories.PushBack(factory);
        }

        return factories;
    }

    void RegisterFactory(TypeId typeId, Handle<UIElementFactoryBase> (*makeFactoryFunction)(void));

private:
    TypeMap<FactoryInstance> m_elementFactories;
};

HYP_CLASS(Abstract)
class HYP_API UIElementFactoryBase : public ObjectBase
{
    HYP_OBJECT_BODY(UIElementFactoryBase);

public:
    UIElementFactoryBase() = default;
    virtual ~UIElementFactoryBase() = default;

    HYP_METHOD(Scriptable)
    virtual TypeId GetElementTypeId() const;

    HYP_METHOD(Scriptable)
    Handle<UIObject> CreateUIObject(UIObject* parent, const BoxedValue& value, const BoxedValue& context) const;

    HYP_METHOD(Scriptable)
    void UpdateUIObject(UIObject* uiObject, const BoxedValue& value, const BoxedValue& context) const;

protected:
    virtual TypeId GetElementTypeId_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* parent, const BoxedValue& value, const BoxedValue& context) const
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void UpdateUIObject_Impl(UIObject* uiObject, const BoxedValue& value, const BoxedValue& context) const
    {
        HYP_PURE_VIRTUAL();
    }
};

template <class T>
class UIElementFactory : public UIElementFactoryBase
{
public:
    virtual ~UIElementFactory() = default;

protected:
    virtual Handle<UIObject> Create(UIObject* parent, const T& value) const = 0;
    virtual void Update(UIObject* uiObject, const T& value) const = 0;

private:
    virtual TypeId GetElementTypeId_Impl() const override final
    {
        return TypeId::ForType<T>();
    }

    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* parent, const BoxedValue& value, const BoxedValue& context) const override final
    {
        HYP_MT_CHECK_RW(m_contextDataRaceDetector);

        m_context = context.ToRef();
        HYP_DEFER({ m_context = AnyRef(); });

        Handle<UIObject> result;

        if constexpr (IsBoxedValueV<T>)
        {
            return Create(parent, value);
        }
        else
        {
            return Create(parent, value.Get<T>());
        }
    }

    virtual void UpdateUIObject_Impl(UIObject* uiObject, const BoxedValue& value, const BoxedValue& context) const override final
    {
        HYP_MT_CHECK_RW(m_contextDataRaceDetector);

        m_context = context.ToRef();
        HYP_DEFER({ m_context = AnyRef(); });

        if constexpr (IsBoxedValueV<T>)
        {
            return Update(uiObject, value);
        }
        else
        {
            return Update(uiObject, value.Get<T>());
        }
    }

protected:
    template <class ContextType>
    const ContextType* GetContext() const
    {
        HYP_MT_CHECK_READ(m_contextDataRaceDetector);

        return m_context.TryGet<ContextType>();
    }

private:
    mutable AnyRef m_context;
    DataRaceDetector m_contextDataRaceDetector;
};

class HYP_API UIDataSourceElement
{
public:
    template <class T, typename = std::enable_if_t<!IsBoxedValueV<T>>>
    UIDataSourceElement(Uuid uuid, T&& value)
        : m_uuid(uuid),
          m_value(BoxedValue(std::forward<T>(value)))
    {
    }

    UIDataSourceElement(Uuid uuid, BoxedValue&& value)
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
        other.m_uuid = Uuid::Invalid();
    }

    UIDataSourceElement& operator=(UIDataSourceElement&& other) noexcept
    {
        if (this != &other)
        {
            m_uuid = other.m_uuid;
            m_value = std::move(other.m_value);

            other.m_uuid = Uuid::Invalid();
        }

        return *this;
    }

    ~UIDataSourceElement() = default;

    HYP_FORCE_INLINE Uuid GetUUID() const
    {
        return m_uuid;
    }

    HYP_FORCE_INLINE BoxedValue& GetValue()
    {
        return m_value;
    }

    HYP_FORCE_INLINE const BoxedValue& GetValue() const
    {
        return m_value;
    }

private:
    Uuid m_uuid;
    BoxedValue m_value;
};

HYP_CLASS(Abstract)
class HYP_API UIDataSourceBase : public ObjectBase
{
    HYP_OBJECT_BODY(UIDataSourceBase);

protected:
    UIDataSourceBase(const Array<Handle<UIElementFactoryBase>>& list)
    {
        for (const Handle<UIElementFactoryBase>& factory : list)
        {
            AssertDebug(factory != nullptr);

            m_elementFactories.Set(factory->GetElementTypeId(), factory);
        }
    }

public:
    UIDataSourceBase(const UIDataSourceBase& other) = delete;
    UIDataSourceBase& operator=(const UIDataSourceBase& other) = delete;
    UIDataSourceBase(UIDataSourceBase&& other) noexcept = delete;
    UIDataSourceBase& operator=(UIDataSourceBase&& other) noexcept = delete;
    virtual ~UIDataSourceBase() = default;

    virtual Result Push(const Uuid& uuid, BoxedValue&& value, const Uuid& parentUuid = Uuid::Invalid()) = 0;
    virtual const UIDataSourceElement* Get(const Uuid& uuid) const = 0;
    virtual void Set(const Uuid& uuid, BoxedValue&& value) = 0;
    virtual void ForceUpdate(const Uuid& uuid) = 0;
    virtual bool Remove(const Uuid& uuid) = 0;
    virtual void RemoveAllWithPredicate(ProcRef<bool(UIDataSourceElement*)> predicate) = 0;

    virtual Handle<UIObject> CreateUIObject(UIObject* parent, const BoxedValue& value, const BoxedValue& context) const = 0;
    virtual void UpdateUIObject(UIObject* uiObject, const BoxedValue& value, const BoxedValue& context) const = 0;

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
    TypeMap<Handle<UIElementFactoryBase>> m_elementFactories;
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
    // temp : required for Class
    UIDataSource()
        : UIDataSourceBase({})
    {
    }

    template <class... Types>
    UIDataSource(TypeWrapper<Types>...)
        : UIDataSource(UIElementFactoryRegistry::GetInstance().GetFactories<Types...>())
    {
    }

    UIDataSource(const Array<Handle<UIElementFactoryBase>>& factories)
        : UIDataSourceBase(factories)
    {
    }

    template <class CreateUIObjectFunction, class UpdateUIObjectFunction>
    UIDataSource(const Array<Handle<UIElementFactoryBase>>& factories, CreateUIObjectFunction&& createUiObject, UpdateUIObjectFunction&& updateUiObject)
        : UIDataSourceBase(factories),
          m_createUiObjectProc(std::forward<CreateUIObjectFunction>(createUiObject)),
          m_updateUiObjectProc(std::forward<UpdateUIObjectFunction>(updateUiObject))
    {
    }

    UIDataSource(const UIDataSource& other) = delete;
    UIDataSource& operator=(const UIDataSource& other) = delete;
    UIDataSource(UIDataSource&& other) noexcept = delete;
    UIDataSource& operator=(UIDataSource&& other) noexcept = delete;
    virtual ~UIDataSource() override = default;

    virtual Result Push(const Uuid& uuid, BoxedValue&& value, const Uuid& parentUuid) override
    {
        if (value.IsNull())
        {
            return HYP_MAKE_ERROR(Error, "Cannot add null value to data source");
        }

        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it != m_values.End())
        {
            return HYP_MAKE_ERROR(Error, "Element with uuid {} already exists in the data source", uuid.ToString());
        }

        typename Forest<UIDataSourceElement>::ConstIterator parentIt = m_values.End();

        if (parentUuid != Uuid::Invalid())
        {
            parentIt = m_values.FindIf([&parentUuid](const auto& item)
                {
                    return item.GetUUID() == parentUuid;
                });

            AssertDebug(parentIt != m_values.End(), "Element with uuid {} not found to set as parent!", parentUuid);
        }

        it = m_values.Add(UIDataSourceElement(uuid, std::move(value)), parentIt);

        OnElementAdd(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);

        return {};
    }

    virtual const UIDataSourceElement* Get(const Uuid& uuid) const override
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

    virtual void Set(const Uuid& uuid, BoxedValue&& value) override
    {
        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it == m_values.End())
        {
            HYP_FAIL("Element with uuid {} not found", uuid.ToString().Data());
        }

        *it = UIDataSourceElement(uuid, std::move(value));

        OnElementUpdate(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);
    }

    virtual void ForceUpdate(const Uuid& uuid) override
    {
        auto it = m_values.FindIf([&uuid](const auto& item)
            {
                return item.GetUUID() == uuid;
            });

        if (it == m_values.End())
        {
            HYP_FAIL("Element with uuid {} not found", uuid.ToString().Data());
        }

        OnElementUpdate(this, &*it, GetParentElementFromIterator(it));
        OnChange(this);
    }

    virtual bool Remove(const Uuid& uuid) override
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

    virtual Handle<UIObject> CreateUIObject(UIObject* parent, const BoxedValue& value, const BoxedValue& context) const override
    {
        if (m_createUiObjectProc.IsValid())
        {
            return m_createUiObjectProc(parent, value, context);
        }

        Handle<UIElementFactoryBase> elementFactory = GetElementFactoryForType(value.ToRef().GetTypeId());

        if (elementFactory)
        {
            return elementFactory->CreateUIObject(parent, value, context);
        }
        else
        {
            HYP_FAIL("No element factory registered for the data source; unable to create UIObject for type `{}`", LookupTypeName(value.ToRef().GetTypeId()));
        }

        return nullptr;
    }

    virtual void UpdateUIObject(UIObject* uiObject, const BoxedValue& value, const BoxedValue& context) const override
    {
        if (m_updateUiObjectProc.IsValid())
        {
            m_updateUiObjectProc(uiObject, value, context);

            return;
        }

        Handle<UIElementFactoryBase> elementFactory = GetElementFactoryForType(value.ToRef().GetTypeId());

        if (elementFactory)
        {
            elementFactory->UpdateUIObject(uiObject, value, context);
        }
        else
        {
            HYP_FAIL("No element factory registered for the data source; unable to udpate UIObject for type {}", LookupTypeName(value.ToRef().GetTypeId()));
        }
    }

    void SetElementFactory(TypeId typeId, const Handle<UIElementFactoryBase>& elementFactory)
    {
        AssertDebug(elementFactory != nullptr);

        if (!elementFactory)
        {
            return;
        }

        m_elementFactories.Set(typeId, elementFactory);
    }

    HYP_METHOD()
    virtual int Size() const override
    {
        return int(m_values.Size());
    }

    HYP_METHOD()
    virtual void Clear() override
    {
        /// \todo check if delegate has any functions bound,
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

private:
    Handle<UIElementFactoryBase> GetElementFactoryForType(TypeId typeId) const;

    Forest<UIDataSourceElement> m_values;

    Proc<Handle<UIObject>(UIObject*, const BoxedValue&, const BoxedValue&)> m_createUiObjectProc;
    Proc<void(UIObject*, const BoxedValue&, const BoxedValue&)> m_updateUiObjectProc;
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

} // namespace Hyperion

#define HYP_DEFINE_UI_ELEMENT_FACTORY(T, Factory)                                        \
    static ::Hyperion::UIElementFactoryRegistration<T> HYP_UNIQUE_NAME(UIElementFactory) \
    {                                                                                    \
        []() -> Handle<UIElementFactoryBase> {                                           \
            return MakeHandle<Factory>();                                              \
        }                                                                                \
    }
