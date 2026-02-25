/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/TypeInfoFwd.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/TypeMap.hpp>

#include <Core/Name.hpp>

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/ObjectFwd.hpp>

#include <scene/ComponentFactory.hpp>
#include <scene/ComponentContainer.hpp>

namespace Hyperion {

class ComponentInterfaceRegistry;
class ComponentContainerFactoryBase;

enum class EntityTag : uint64;

template <EntityTag Tag>
struct TagComponent;

extern HYP_API bool ComponentInterface_CreateInstance(const Class* cls, BoxedValue& outBoxed);

enum class ComponentInterfaceFlags : uint32
{
    NONE = 0x0,
    ENTITY_TAG = 0x1
};

HYP_MAKE_ENUM_FLAGS(ComponentInterfaceFlags)

namespace Attributes {
HYP_API extern const Name g_attrSerialize;
} // namespace Attributes

class HYP_API IComponentInterface
{
public:
    virtual ~IComponentInterface() = default;

    const Class* GetClass() const;

    virtual const TypeInfo& GetTypeInfo() const = 0;
    virtual ComponentContainerFactoryBase* GetComponentContainerFactory() const = 0;

    virtual bool CreateInstance(BoxedValue& out) const = 0;

    virtual bool GetShouldSerialize() const = 0;

    virtual bool IsEntityTag() const = 0;
    virtual EntityTag GetEntityTag() const = 0;
};

template <class Component, bool ShouldSerialize = true>
class ComponentInterface : public IComponentInterface
{
public:
    ComponentInterface()
        : m_componentFactory(nullptr),
          m_componentContainerFactory(nullptr)
    {
    }

    ComponentInterface(UniquePtr<IComponentFactory>&& componentFactory, ComponentContainerFactoryBase* componentContainerFactory)
        : m_componentFactory(std::move(componentFactory)),
          m_componentContainerFactory(componentContainerFactory)
    {
    }

    ComponentInterface(const ComponentInterface&) = delete;
    ComponentInterface& operator=(const ComponentInterface&) = delete;

    ComponentInterface(ComponentInterface&& other) noexcept
        : m_componentFactory(std::move(other.m_componentFactory)),
          m_componentContainerFactory(other.m_componentContainerFactory)
    {
        other.m_componentContainerFactory = nullptr;
    }

    ComponentInterface& operator=(ComponentInterface&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_componentFactory = std::move(other.m_componentFactory);
        m_componentContainerFactory = other.m_componentContainerFactory;

        other.m_componentContainerFactory = nullptr;

        return *this;
    }

    virtual ~ComponentInterface() override = default;

    virtual const TypeInfo& GetTypeInfo() const override
    {
        return TypeOf<Component>();
    }

    virtual ComponentContainerFactoryBase* GetComponentContainerFactory() const override
    {
        return m_componentContainerFactory;
    }

    virtual bool CreateInstance(BoxedValue& out) const override
    {
        return ComponentInterface_CreateInstance(GetClass(), out);
    }

    virtual bool GetShouldSerialize() const override
    {
        return ShouldSerialize && GetClass() && GetClass()->GetAttribute(Attributes::g_attrSerialize) != false;
    }

    virtual bool IsEntityTag() const override
    {
        return false;
    }

    virtual EntityTag GetEntityTag() const override
    {
        return (EntityTag)-1;
    }

private:
    UniquePtr<IComponentFactory> m_componentFactory;
    ComponentContainerFactoryBase* m_componentContainerFactory;
};

template <EntityTag Tag, bool ShouldSerialize = true>
class EntityTagComponentInterface : public IComponentInterface
{
public:
    EntityTagComponentInterface()
        : m_componentFactory(nullptr),
          m_componentContainerFactory(nullptr)
    {
    }

    EntityTagComponentInterface(UniquePtr<IComponentFactory>&& componentFactory, ComponentContainerFactoryBase* componentContainerFactory)
        : m_componentFactory(std::move(componentFactory)),
          m_componentContainerFactory(componentContainerFactory)
    {
    }

    EntityTagComponentInterface(const EntityTagComponentInterface&) = delete;
    EntityTagComponentInterface& operator=(const EntityTagComponentInterface&) = delete;

    EntityTagComponentInterface(EntityTagComponentInterface&& other) noexcept
        : m_componentFactory(std::move(other.m_componentFactory)),
          m_componentContainerFactory(other.m_componentContainerFactory)
    {
        other.m_componentContainerFactory = nullptr;
    }

    EntityTagComponentInterface& operator=(EntityTagComponentInterface&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_componentFactory = std::move(other.m_componentFactory);
        m_componentContainerFactory = other.m_componentContainerFactory;

        other.m_componentContainerFactory = nullptr;

        return *this;
    }

    virtual ~EntityTagComponentInterface() override = default;

    virtual const TypeInfo& GetTypeInfo() const override
    {
        return TypeOf<TagComponent<Tag>>();
    }

    virtual ComponentContainerFactoryBase* GetComponentContainerFactory() const override
    {
        return m_componentContainerFactory;
    }

    virtual bool CreateInstance(BoxedValue& out) const override
    {
        out = BoxedValue(TagComponent<Tag> {});

        return true;
    }

    virtual bool GetShouldSerialize() const override
    {
        return ShouldSerialize;
    }

    virtual bool IsEntityTag() const override
    {
        return true;
    }

    virtual EntityTag GetEntityTag() const override
    {
        return Tag;
    }

private:
    UniquePtr<IComponentFactory> m_componentFactory;
    ComponentContainerFactoryBase* m_componentContainerFactory;
};

class ComponentInterfaceRegistry
{
public:
    static ComponentInterfaceRegistry& GetInstance();

    ComponentInterfaceRegistry();

    void Initialize();
    void Shutdown();

    void Register(TypeId typeId, UniquePtr<IComponentInterface> (*fptr)());

    const IComponentInterface* GetComponentInterface(TypeId typeId) const
    {
        Assert(m_isInitialized, "Component interface registry not initialized!");

        auto it = m_interfaces.Find(typeId);

        if (it == m_interfaces.End())
        {
            return nullptr;
        }

        return it->second.Get();
    }

    Array<const IComponentInterface*> GetComponentInterfaces() const
    {
        Assert(m_isInitialized, "Component interface registry not initialized!");

        Array<const IComponentInterface*> interfaces;
        interfaces.Resize(m_interfaces.Size());

        uint32 interfaceIndex = 0;

        for (auto it = m_interfaces.Begin(); it != m_interfaces.End(); ++it, ++interfaceIndex)
        {
            interfaces[interfaceIndex] = it->second.Get();
        }

        return interfaces;
    }

    const IComponentInterface* GetEntityTagComponentInterface(EntityTag tag) const
    {
        Assert(m_isInitialized, "Component interface registry not initialized!");

        for (auto it = m_interfaces.Begin(); it != m_interfaces.End(); ++it)
        {
            if (it->second->IsEntityTag() && it->second->GetEntityTag() == tag)
            {
                return it->second.Get();
            }
        }

        return nullptr;
    }

private:
    bool m_isInitialized;
    TypeMap<UniquePtr<IComponentInterface> (*)()> m_factories;
    TypeMap<UniquePtr<IComponentInterface>> m_interfaces;
};

template <class ComponentType, bool ShouldSerialize = true>
struct ComponentInterfaceRegistration
{
    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeId::ForType<ComponentType>(),
            []() -> UniquePtr<IComponentInterface>
            {
                return MakeUnique<ComponentInterface<ComponentType, ShouldSerialize>>(
                    MakeUnique<ComponentFactory<ComponentType>>(),
                    ComponentContainer<ComponentType>::GetFactory());
            });
    }
};

template <EntityTag Tag, bool ShouldSerialize>
struct ComponentInterfaceRegistration<TagComponent<Tag>, ShouldSerialize>
{
    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeId::ForType<TagComponent<Tag>>(),
            []() -> UniquePtr<IComponentInterface>
            {
                return MakeUnique<EntityTagComponentInterface<Tag, ShouldSerialize>>(
                    MakeUnique<ComponentFactory<TagComponent<Tag>>>(),
                    ComponentContainer<TagComponent<Tag>>::GetFactory());
            });
    }
};

#define HYP_REGISTER_COMPONENT(type, ...)                                                             \
    static ComponentInterfaceRegistration<type, ##__VA_ARGS__> type##_ComponentInterface_Registration \
    {                                                                                                 \
    }
#define HYP_REGISTER_ENTITY_TAG(tag, ...)                                                                                              \
    static ComponentInterfaceRegistration<TagComponent<EntityTag::tag>, ##__VA_ARGS__> tag##_EntityTag_ComponentInterface_Registration \
    {                                                                                                                                  \
    }

#define HYP_REGISTER_ENTITY_TYPE(T, ...)                                                                                                               \
    static ComponentInterfaceRegistration<TagComponent<EntityType_Impl<T>::value>, false, ##__VA_ARGS__> T##_EntityTag_ComponentInterface_Registration \
    {                                                                                                                                                  \
    }

} // namespace Hyperion
