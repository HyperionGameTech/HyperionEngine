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

namespace hyperion {

class ComponentInterfaceBase;
class ComponentInterfaceRegistry;

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

    Name GetName() const
        { return m_name; }

    HYP_FORCE_INLINE
    bool IsReadable() const
        { return m_flags & ComponentPropertyFlags::READ; }

    HYP_FORCE_INLINE
    bool IsWritable() const
        { return m_flags & ComponentPropertyFlags::WRITE; }

    HYP_FORCE_INLINE
    bool IsReadOnly() const
        { return !IsWritable(); }

    HYP_FORCE_INLINE
    const Getter &GetGetter() const
        { return m_getter; }

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

private:
    bool                                                        m_is_initialized;
    HashMap< TypeID, UniquePtr< ComponentInterfaceBase >(*)() > m_factories;
    HashMap< TypeID, UniquePtr< ComponentInterfaceBase > >      m_interfaces;
};


class ComponentInterfaceBase
{
public:
    ComponentInterfaceBase();

    ComponentInterfaceBase(const ComponentInterfaceBase &)                  = delete;
    ComponentInterfaceBase &operator=(const ComponentInterfaceBase &)       = delete;

    ComponentInterfaceBase(ComponentInterfaceBase &&) noexcept              = delete;
    ComponentInterfaceBase &operator=(ComponentInterfaceBase &&) noexcept   = delete;

    virtual ~ComponentInterfaceBase()                                       = default;

    /*! \brief Called by ComponentInterfaceRegistry, not to be called by user code */
    void Initialize();

    TypeID GetTypeID() const
        { return m_type_id; }

    const Array<ComponentProperty> &GetProperties() const
        { return m_properties; }

    ComponentProperty *GetProperty(WeakName name);
    const ComponentProperty *GetProperty(WeakName name) const;

protected:
    virtual TypeID GetTypeID_Internal() const = 0;
    virtual Array<ComponentProperty> GetProperties_Internal() const = 0;

private:
    TypeID                      m_type_id;
    Array<ComponentProperty>    m_properties;
};

template <class ComponentType>
struct ComponentInterfaceRegistration
{
    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(TypeID::ForType<ComponentType>(), []() -> UniquePtr<ComponentInterfaceBase>
        {
            return UniquePtr<ComponentInterfaceBase>(new ComponentInterface<ComponentType>());
        });
    }
};

#define HYP_REGISTER_COMPONENT_INTERFACE(type) static ComponentInterfaceRegistration< type > type##_ComponentInterface_Registration { }

} // namespace hyperion

#endif