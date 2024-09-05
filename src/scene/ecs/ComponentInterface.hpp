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

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>
#include <math/Quaternion.hpp>

#include <scene/ecs/ComponentFactory.hpp>
#include <scene/ecs/ComponentContainer.hpp>

namespace hyperion {

class ComponentInterfaceRegistry;
class ComponentContainerFactoryBase;

struct HypData;

class ComponentInterface
{
public:
    ComponentInterface()
        : m_type_id(TypeID::Void()),
          m_component_factory(nullptr),
          m_component_container_factory(nullptr)
    {
    }

    ComponentInterface(TypeID type_id, UniquePtr<ComponentFactoryBase> &&component_factory, ComponentContainerFactoryBase *component_container_factory)
        : m_type_id(type_id),
          m_component_factory(std::move(component_factory)),
          m_component_container_factory(component_container_factory)
    {
    }

    ComponentInterface(const ComponentInterface &)                  = delete;
    ComponentInterface &operator=(const ComponentInterface &)       = delete;

    ComponentInterface(ComponentInterface &&other) noexcept
        : m_type_id(other.m_type_id),
          m_component_factory(std::move(other.m_component_factory)),
          m_component_container_factory(other.m_component_container_factory)
    {
        other.m_type_id = TypeID::Void();
        other.m_component_container_factory = nullptr;
    }

    ComponentInterface &operator=(ComponentInterface &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        m_type_id = other.m_type_id;
        m_component_factory = std::move(other.m_component_factory);
        m_component_container_factory = other.m_component_container_factory;

        other.m_type_id = TypeID::Void();
        other.m_component_container_factory = nullptr;

        return *this;
    }

    ~ComponentInterface()                                           = default;

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    HYP_FORCE_INLINE HypData CreateComponent() const
        { return m_component_factory->CreateComponent(); }

    HYP_FORCE_INLINE ComponentContainerFactoryBase *GetComponentContainerFactory() const
        { return m_component_container_factory; }

private:
    TypeID                          m_type_id;
    UniquePtr<ComponentFactoryBase> m_component_factory;
    ComponentContainerFactoryBase   *m_component_container_factory;
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

template <class ComponentType>
struct ComponentInterfaceRegistration;

template <class ComponentType>
struct ComponentInterfaceRegistration
{
    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeID::ForType<ComponentType>(),
            TypeNameWithoutNamespace<ComponentType>(),
            []() -> ComponentInterface
            {
                return ComponentInterface(
                    TypeID::ForType<ComponentType>(),
                    UniquePtr<ComponentFactoryBase>(new ComponentFactory<ComponentType>()),
                    ComponentContainer<ComponentType>::GetFactory()
                );
            }
        );
    }
};

#define HYP_REGISTER_COMPONENT(type) static ComponentInterfaceRegistration< type > type##_ComponentInterface_Registration { }
#define HYP_REGISTER_ENTITY_TAG(tag) static ComponentInterfaceRegistration< EntityTagComponent< EntityTag::tag > > tag##_EntityTag_ComponentInterface_Registration { }

} // namespace hyperion

#endif