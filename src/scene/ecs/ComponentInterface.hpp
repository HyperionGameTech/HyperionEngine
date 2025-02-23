/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_COMPONENT_INTERFACE_HPP
#define HYPERION_ECS_COMPONENT_INTERFACE_HPP

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/Name.hpp>

#include <asset/serialization/Serialization.hpp>

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/Quaternion.hpp>

#include <scene/ecs/ComponentFactory.hpp>
#include <scene/ecs/ComponentContainer.hpp>

namespace hyperion {


class ComponentInterfaceRegistry;
class ComponentContainerFactoryBase;

struct HypData;
class HypClass;

extern HYP_API const HypClass *GetClass(TypeID);

enum class ComponentInterfaceFlags : uint32
{
    NONE        = 0x0,
    ENTITY_TAG  = 0x1
};

HYP_MAKE_ENUM_FLAGS(ComponentInterfaceFlags)

class ComponentInterface
{
public:
    ComponentInterface()
        : m_type_id(TypeID::Void()),
          m_component_factory(nullptr),
          m_component_container_factory(nullptr),
          m_flags(ComponentInterfaceFlags::NONE)
    {
    }

    ComponentInterface(TypeID type_id, ANSIStringView type_name, UniquePtr<IComponentFactory> &&component_factory, ComponentContainerFactoryBase *component_container_factory, EnumFlags<ComponentInterfaceFlags> flags)
        : m_type_id(type_id),
          m_type_name(type_name),
          m_component_factory(std::move(component_factory)),
          m_component_container_factory(component_container_factory),
          m_flags(flags)
    {
        if (!(m_flags & ComponentInterfaceFlags::ENTITY_TAG)) {
            AssertThrowMsg(::hyperion::GetClass(m_type_id) != nullptr, "No HypClass registered for Component of type %s", type_name.Data());
        }
    }

    ComponentInterface(const ComponentInterface &)                  = delete;
    ComponentInterface &operator=(const ComponentInterface &)       = delete;

    ComponentInterface(ComponentInterface &&other) noexcept
        : m_type_id(other.m_type_id),
          m_type_name(std::move(other.m_type_name)),
          m_component_factory(std::move(other.m_component_factory)),
          m_component_container_factory(other.m_component_container_factory),
          m_flags(other.m_flags)
    {
        other.m_type_id = TypeID::Void();
        other.m_component_container_factory = nullptr;
        other.m_flags = ComponentInterfaceFlags::NONE;
    }

    ComponentInterface &operator=(ComponentInterface &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        m_type_id = other.m_type_id;
        m_type_name = std::move(other.m_type_name);
        m_component_factory = std::move(other.m_component_factory);
        m_component_container_factory = other.m_component_container_factory;
        m_flags = other.m_flags;

        other.m_type_id = TypeID::Void();
        other.m_component_container_factory = nullptr;
        other.m_flags = ComponentInterfaceFlags::NONE;

        return *this;
    }

    ~ComponentInterface()                                           = default;

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    HYP_FORCE_INLINE ANSIStringView GetTypeName() const
        { return m_type_name; }

    HYP_FORCE_INLINE const HypClass *GetClass() const
        { return ::hyperion::GetClass(m_type_id); }

    HYP_FORCE_INLINE ComponentContainerFactoryBase *GetComponentContainerFactory() const
        { return m_component_container_factory; }

    HYP_FORCE_INLINE EnumFlags<ComponentInterfaceFlags> GetFlags() const
        { return m_flags; }

private:
    TypeID                              m_type_id;
    ANSIStringView                      m_type_name;
    UniquePtr<IComponentFactory>        m_component_factory;
    ComponentContainerFactoryBase       *m_component_container_factory;
    EnumFlags<ComponentInterfaceFlags>  m_flags;
};

class ComponentInterfaceRegistry
{
public:
    static ComponentInterfaceRegistry &GetInstance();

    ComponentInterfaceRegistry();

    void Initialize();
    void Shutdown();

    void Register(TypeID type_id, ANSIStringView type_name, ComponentInterface(*fptr)());

    HYP_FORCE_INLINE const ComponentInterface *GetComponentInterface(TypeID type_id) const
    {
        AssertThrowMsg(m_is_initialized, "Component interface registry not initialized!");

        auto it = m_interfaces.Find(type_id);

        if (it == m_interfaces.End()) {
            return nullptr;
        }

        return &it->second;
    }

    HYP_FORCE_INLINE Array<const ComponentInterface *> GetComponentInterfaces() const
    {
        AssertThrowMsg(m_is_initialized, "Component interface registry not initialized!");

        Array<const ComponentInterface *> interfaces;
        interfaces.Resize(m_interfaces.Size());

        uint32 interface_index = 0;

        for (auto it = m_interfaces.Begin(); it != m_interfaces.End(); ++it, ++interface_index) {
            interfaces[interface_index] = &it->second;
        }

        return interfaces;
    }

private:
    bool                                m_is_initialized;
    TypeMap<ComponentInterface(*)()>    m_factories;
    TypeMap<ComponentInterface>         m_interfaces;
};

template <class ComponentType, ComponentInterfaceFlags... Flags>
struct ComponentInterfaceRegistration
{
    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeID::ForType<ComponentType>(),
            TypeNameHelper<ComponentType, true>::value,
            []() -> ComponentInterface
            {
                return ComponentInterface(
                    TypeID::ForType<ComponentType>(),
                    TypeNameHelper<ComponentType, true>::value,
                    MakeUnique<ComponentFactory<ComponentType>>(),
                    ComponentContainer<ComponentType>::GetFactory(),
                    MergeEnumFlags<ComponentInterfaceFlags, Flags...>::value
                );
            }
        );
    }
};

#define HYP_REGISTER_COMPONENT(type, ...) static ComponentInterfaceRegistration< type, ##__VA_ARGS__ > type##_ComponentInterface_Registration { }
#define HYP_REGISTER_ENTITY_TAG(tag) static ComponentInterfaceRegistration< EntityTagComponent<EntityTag::tag>, ComponentInterfaceFlags::ENTITY_TAG > tag##_EntityTag_ComponentInterface_Registration { }

} // namespace hyperion

#endif