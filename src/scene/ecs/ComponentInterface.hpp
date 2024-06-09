/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_COMPONENT_INTERFACE_HPP
#define HYPERION_ECS_COMPONENT_INTERFACE_HPP

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/containers/Array.hpp>
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

class ComponentInterfaceBase;
class ComponentInterfaceRegistry;
class ComponentContainerFactoryBase;

template <class ComponentType>
class ComponentInterface;

enum class ComponentPropertyFlags : uint32
{
    NONE        = 0x0,
    READ        = 0x1,
    WRITE       = 0x2,
    READ_WRITE  = READ | WRITE
};

HYP_MAKE_ENUM_FLAGS(ComponentPropertyFlags)

class ComponentProperty
{
public:
    using Getter = void(*)(const void *component, fbom::FBOMData *out);
    using Setter = void(*)(void *component, const fbom::FBOMData *in);

    ComponentProperty()
        : m_name(Name::Invalid()),
          m_flags(ComponentPropertyFlags::NONE),
          m_getter(nullptr),
          m_setter(nullptr)
    {
    }

    ComponentProperty(Name name, Getter &&getter)
        : m_name(name),
          m_flags(ComponentPropertyFlags::READ),
          m_getter(std::move(getter)),
          m_setter(nullptr)
    {
    }

    ComponentProperty(Name name, Getter &&getter, Setter &&setter)
        : m_name(name),
          m_flags(ComponentPropertyFlags::READ_WRITE),
          m_getter(std::move(getter)),
          m_setter(std::move(setter))
    {
    }

    ComponentProperty(const ComponentProperty &)                = default;
    ComponentProperty &operator=(const ComponentProperty &)     = default;

    ComponentProperty(ComponentProperty &&) noexcept            = default;
    ComponentProperty &operator=(ComponentProperty &&) noexcept = default;

    ~ComponentProperty()                                        = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    Name GetName() const
        { return m_name; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    EnumFlags<ComponentPropertyFlags> GetFlags() const
        { return m_flags; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsReadable() const
        { return m_flags & ComponentPropertyFlags::READ; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsWritable() const
        { return m_flags & ComponentPropertyFlags::WRITE; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsReadOnly() const
        { return !IsWritable(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Getter &GetGetter() const
        { return m_getter; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Setter &GetSetter() const
        { return m_setter; }

private:
    Name                                m_name;
    EnumFlags<ComponentPropertyFlags>   m_flags;
    Getter                              m_getter;
    Setter                              m_setter;
};

static_assert(sizeof(ComponentProperty) == 32, "sizeof(ComponentProperty) must match C# struct size");

template <class ComponentType>
class ComponentInterface;

class ComponentInterfaceRegistry
{
public:
    static ComponentInterfaceRegistry &GetInstance();

    ComponentInterfaceRegistry();

    void Initialize();
    void Shutdown();

    void Register(TypeID component_type_id, UniquePtr< ComponentInterfaceBase >(*fptr)());

    [[nodiscard]]
    HYP_FORCE_INLINE
    ComponentInterfaceBase *GetComponentInterface(TypeID type_id) const
    {
        AssertThrowMsg(m_is_initialized, "Component interface registry not initialized!");

        auto it = m_interfaces.Find(type_id);

        if (it == m_interfaces.End()) {
            return nullptr;
        }

        return it->second.Get();
    }

    template <class T>
    [[nodiscard]]
    HYP_FORCE_INLINE
    ComponentInterface<T> *GetComponentInterface() const
    {
        return static_cast< ComponentInterface<T> *>(GetComponentInterface(TypeID::ForType<T>()));
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Array<ComponentInterfaceBase *> GetComponentInterfaces() const
    {
        AssertThrowMsg(m_is_initialized, "Component interface registry not initialized!");

        Array<ComponentInterfaceBase *> interfaces;
        interfaces.Resize(m_interfaces.Size());

        uint32 interface_index = 0;

        for (auto it = m_interfaces.Begin(); it != m_interfaces.End(); ++it, ++interface_index) {
            interfaces[interface_index] = it->second.Get();
        }

        return interfaces;
    }

private:
    bool                                                        m_is_initialized;
    HashMap< TypeID, UniquePtr< ComponentInterfaceBase >(*)() > m_factories;
    HashMap< TypeID, UniquePtr< ComponentInterfaceBase > >      m_interfaces;
};

template <class ComponentType>
struct ComponentInterfaceRegistration;

class ComponentInterfaceBase
{
protected:
    ComponentInterfaceBase();

public:
    template <class ComponentType>
    friend struct ComponentInterfaceRegistration;

    ComponentInterfaceBase(const ComponentInterfaceBase &)                  = delete;
    ComponentInterfaceBase &operator=(const ComponentInterfaceBase &)       = delete;

    ComponentInterfaceBase(ComponentInterfaceBase &&) noexcept              = delete;
    ComponentInterfaceBase &operator=(ComponentInterfaceBase &&) noexcept   = delete;

    virtual ~ComponentInterfaceBase()                                       = default;

    /*! \brief Called by ComponentInterfaceRegistry, not to be called by user code */
    void Initialize();

    UniquePtr<void> CreateComponent() const
        { return m_component_factory->CreateComponent(); }

    ComponentContainerFactoryBase *GetComponentContainerFactory() const
        { return m_component_container_factory; }

    TypeID GetTypeID() const
        { return m_type_id; }

    virtual ANSIStringView GetTypeName() const final
        { return GetTypeName_Internal(); }

    const Array<ComponentProperty> &GetProperties() const
        { return m_properties; }

    ComponentProperty *GetProperty(WeakName name);
    const ComponentProperty *GetProperty(WeakName name) const;

protected:
    virtual TypeID GetTypeID_Internal() const = 0;
    virtual ANSIStringView GetTypeName_Internal() const = 0;

    virtual Array<ComponentProperty> GetProperties_Internal() const = 0;

private:
    UniquePtr<ComponentFactoryBase> m_component_factory;
    ComponentContainerFactoryBase   *m_component_container_factory;
    TypeID                          m_type_id;
    Array<ComponentProperty>        m_properties;
};

template <class ComponentType>
struct ComponentInterfaceRegistration
{
    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(TypeID::ForType<ComponentType>(), []() -> UniquePtr<ComponentInterfaceBase>
        {
            UniquePtr<ComponentInterfaceBase> component_interface(new ComponentInterface<ComponentType>());
            component_interface->m_component_factory = UniquePtr<ComponentFactoryBase>(new ComponentFactory<ComponentType>());
            component_interface->m_component_container_factory = ComponentContainer<ComponentType>::GetFactory();

            return component_interface;
        });
    }
};

#define HYP_REGISTER_COMPONENT_INTERFACE(type) static ComponentInterfaceRegistration< type > type##_ComponentInterface_Registration { }

} // namespace hyperion

#endif