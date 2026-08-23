/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/TypeInfoFwd.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/TypeMap.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/ObjectFwd.hpp>
#include <Core/Reflection/ClassAttribute.hpp>

#include <Scene/ComponentFactory.hpp>
#include <Scene/ComponentContainer.hpp>
#include <Scene/EntityTag.hpp>

namespace Hyperion {

class ComponentInterfaceRegistry;
class ComponentContainerFactoryBase;

template <EntityTag Tag>
struct TagComponent;

extern "C" ENGINE_API bool ComponentInterface_CreateInstance(const Class* cls, BoxedValue& outBoxed);
extern "C" ENGINE_API const ClassAttribute* Class_GetAttribute(const Class* cls, const Name* name);

enum class ComponentInterfaceFlags : uint32
{
    NONE = 0x0,
    ENTITY_TAG = 0x1
};

HYP_MAKE_ENUM_FLAGS(ComponentInterfaceFlags)

namespace Attributes {
CORE_API extern const Name g_attrSerialize;
CORE_API extern const Name g_attrReplicated;
} // namespace Attributes

// Not really an interface, but the name stays for now
class ENGINE_API IComponentInterface
{
protected:
    IComponentInterface()
    {
        m_shouldSerialize = false;
        m_isReplicated = false;
        m_isEntityTag = false;
        m_showInEditor = true;
        m_entityTag = (EntityTag)-1;
    }

public:
    virtual ~IComponentInterface() = default;

    const Class* GetClass() const;

    virtual const TypeInfo& GetTypeInfo() const = 0;
    virtual ComponentContainerFactoryBase* GetComponentContainerFactory() const = 0;

    virtual bool CreateInstance(BoxedValue& out) const = 0;

    HYP_FORCE_INLINE bool GetShouldSerialize() const
    {
        return m_shouldSerialize;
    }

    HYP_FORCE_INLINE bool ShouldShowInEditor() const
    {
        return m_showInEditor;
    }

    HYP_FORCE_INLINE bool IsEntityTag() const
    {
        return m_isEntityTag;
    }

    HYP_FORCE_INLINE EntityTag GetEntityTag() const
    {
        return m_entityTag;
    }

    HYP_FORCE_INLINE bool IsReplicated() const
    {
        return m_isReplicated;
    }

protected:
    UniquePtr<IComponentFactory> m_componentFactory;
    ComponentContainerFactoryBase* m_componentContainerFactory;

    bool m_shouldSerialize : 1;
    bool m_isReplicated : 1;
    bool m_isEntityTag : 1;
    bool m_showInEditor : 1;

    EntityTag m_entityTag;
};

template <class Component, bool ShouldSerialize = true>
class ComponentInterface final : public IComponentInterface
{
public:
    ComponentInterface()
    {
        const auto getClassAttributeValue = [cls = GetClass()](const Name& name) -> const ClassAttributeValue&
        {
            const ClassAttribute* attr = Class_GetAttribute(cls, &name);
            if (!attr)
            {
                return ClassAttributeValue::empty;
            }

            return attr->GetValue();
        };

        m_shouldSerialize = (ShouldSerialize && getClassAttributeValue(Attributes::g_attrSerialize) != false);
        m_showInEditor = m_shouldSerialize;
        m_isReplicated = (getClassAttributeValue(Attributes::g_attrReplicated) != false);
        m_isEntityTag = false;
    }

    ComponentInterface(UniquePtr<IComponentFactory>&& componentFactory, ComponentContainerFactoryBase* componentContainerFactory)
        : ComponentInterface()
    {
        m_componentFactory = std::move(componentFactory);
        m_componentContainerFactory = std::move(componentContainerFactory);
    }

    ComponentInterface(const ComponentInterface&) = delete;
    ComponentInterface& operator=(const ComponentInterface&) = delete;

    ComponentInterface(ComponentInterface&& other) noexcept = delete;
    ComponentInterface& operator=(ComponentInterface&& other) noexcept = delete;

    ~ComponentInterface() override = default;

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
};

template <class ComponentT, EntityTag Tag, bool ShouldSerialize = true, bool ShowInEditor = true>
class EntityTagComponentInterface final : public IComponentInterface
{
public:
    EntityTagComponentInterface()
    {
        m_shouldSerialize = ShouldSerialize;
        m_showInEditor = ShouldSerialize && ShowInEditor;
        m_isReplicated = ShouldSerialize;
        m_isEntityTag = true;
        m_entityTag = Tag;
    }

    EntityTagComponentInterface(UniquePtr<IComponentFactory>&& componentFactory, ComponentContainerFactoryBase* componentContainerFactory)
        : EntityTagComponentInterface()
    {
        m_componentFactory = std::move(componentFactory);
        m_componentContainerFactory = std::move(componentContainerFactory);
    }

    EntityTagComponentInterface(const EntityTagComponentInterface&) = delete;
    EntityTagComponentInterface& operator=(const EntityTagComponentInterface&) = delete;

    virtual ~EntityTagComponentInterface() override = default;

    virtual const TypeInfo& GetTypeInfo() const override
    {
        return TypeOf<ComponentT>();
    }

    virtual ComponentContainerFactoryBase* GetComponentContainerFactory() const override
    {
        return m_componentContainerFactory;
    }

    virtual bool CreateInstance(BoxedValue& out) const override
    {
        out = BoxedValue(ComponentT {});

        return true;
    }
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
    Map<TypeId, UniquePtr<IComponentInterface> (*)()> m_factories;
    Map<TypeId, UniquePtr<IComponentInterface>> m_interfaces;

    bool m_isInitialized;
};

template <class ComponentType, bool ShouldSerialize = true, bool ShowInEditor = true>
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

template <EntityTag Tag, bool ShouldSerialize, bool ShowInEditor>
struct ComponentInterfaceRegistration<TagComponent<Tag>, ShouldSerialize, ShowInEditor>
{
    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeId::ForType<TagComponent<Tag>>(),
            []() -> UniquePtr<IComponentInterface>
            {
                return MakeUnique<EntityTagComponentInterface<TagComponent<Tag>, Tag, ShouldSerialize, ShowInEditor>>(
                    MakeUnique<ComponentFactory<TagComponent<Tag>>>(),
                    ComponentContainer<TagComponent<Tag>>::GetFactory());
            });
    }
};

template <class T, bool ShouldSerialize, bool ShowInEditor>
struct ComponentInterfaceRegistration<EntityTypeTag<T>, ShouldSerialize, ShowInEditor>
{
    static constexpr EntityTag Tag = EntityType_Impl<T>::value;

    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeId::ForType<EntityTypeTag<T>>(),
            []() -> UniquePtr<IComponentInterface>
            {
                return MakeUnique<EntityTagComponentInterface<EntityTypeTag<T>, Tag, ShouldSerialize, ShowInEditor>>(
                    MakeUnique<ComponentFactory<EntityTypeTag<T>>>(),
                    ComponentContainer<EntityTypeTag<T>>::GetFactory());
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
    static ComponentInterfaceRegistration<EntityTypeTag<T>, false, false, ##__VA_ARGS__> T##_EntityTag_ComponentInterface_Registration \
    {                                                                                                                                                  \
    }

} // namespace Hyperion
