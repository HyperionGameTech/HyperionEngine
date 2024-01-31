#ifndef HYPERION_V2_ECS_COMPONENT_INTERFACE_HPP
#define HYPERION_V2_ECS_COMPONENT_INTERFACE_HPP

#include <core/lib/TypeID.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/Variant.hpp>
#include <core/Name.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>
#include <math/Quaternion.hpp>

namespace hyperion::v2 {

class ComponentInterfaceBase;

using ComponentPropertyFlags = UInt32;

enum ComponentPropertyFlagBits : ComponentPropertyFlags
{
    COMPONENT_PROPERTY_FLAG_NONE        = 0x0,
    COMPONENT_PROPERTY_FLAG_READ        = 0x1,
    COMPONENT_PROPERTY_FLAG_WRITE       = 0x2,
    COMPONENT_PROPERTY_FLAG_READ_WRITE  = COMPONENT_PROPERTY_FLAG_READ | COMPONENT_PROPERTY_FLAG_WRITE
};

class ComponentProperty
{
public:
    using Value = Variant<bool, Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64, Float, Double, String, Vec3f, Vec3i, Vec3u, Vec4f, Vec4i, Vec4u, Quaternion, Matrix4>;

    using Getter = std::add_pointer_t<Value (const void *component)>;
    using Setter = std::add_pointer_t<void (void *component, Value &&value)>;

    ComponentProperty()
        : m_name(Name::invalid),
          m_flags(COMPONENT_PROPERTY_FLAG_NONE),
          m_getter(nullptr),
          m_setter(nullptr)
    {
    }

    ComponentProperty(Name name, Getter &&getter)
        : m_name(name),
          m_flags(COMPONENT_PROPERTY_FLAG_READ),
          m_getter(std::move(getter)),
          m_setter(nullptr)
    {
    }

    ComponentProperty(Name name, Getter &&getter, Setter &&setter)
        : m_name(name),
          m_flags(COMPONENT_PROPERTY_FLAG_READ_WRITE),
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
    Bool IsReadable() const
        { return m_flags & COMPONENT_PROPERTY_FLAG_READ; }

    HYP_FORCE_INLINE
    Bool IsWritable() const
        { return m_flags & COMPONENT_PROPERTY_FLAG_WRITE; }

    HYP_FORCE_INLINE
    Bool IsReadOnly() const
        { return !IsWritable(); }

    HYP_FORCE_INLINE
    const Getter &GetGetter() const
        { return m_getter; }

    HYP_FORCE_INLINE
    const Setter &GetSetter() const
        { return m_setter; }

private:
    Name                    m_name;
    ComponentPropertyFlags  m_flags;

    Getter                  m_getter;
    Setter                  m_setter;
};

class ComponentInterfaceBase
{
public:
    ComponentInterfaceBase(TypeID type_id, Array<ComponentProperty> &&properties);

    ComponentInterfaceBase(const ComponentInterfaceBase &)                  = delete;
    ComponentInterfaceBase &operator=(const ComponentInterfaceBase &)       = delete;

    ComponentInterfaceBase(ComponentInterfaceBase &&) noexcept              = delete;
    ComponentInterfaceBase &operator=(ComponentInterfaceBase &&) noexcept   = delete;

    virtual ~ComponentInterfaceBase()                                       = default;

    TypeID GetTypeID() const
        { return m_type_id; }

    const Array<ComponentProperty> &GetProperties() const
        { return m_properties; }

    ComponentProperty *GetProperty(Name name);
    const ComponentProperty *GetProperty(Name name) const;

protected:
    TypeID                      m_type_id;
    Array<ComponentProperty>    m_properties;
};

template <class ComponentType>
class ComponentInterface : public ComponentInterfaceBase
{
public:
    ComponentInterface(Array<ComponentProperty> &&properties)
        : ComponentInterfaceBase(TypeID::ForType<ComponentType>(), std::move(properties))
    {
    }

    ComponentInterface(const ComponentInterface &)                  = delete;
    ComponentInterface &operator=(const ComponentInterface &)       = delete;

    ComponentInterface(ComponentInterface &&) noexcept              = delete;
    ComponentInterface &operator=(ComponentInterface &&) noexcept   = delete;

    virtual ~ComponentInterface() override                          = default;
};

ComponentInterfaceBase *GetComponentInterface(TypeID type_id);

template <class ComponentType>
static inline ComponentInterface<ComponentType> *GetComponentInterface()
{
    static_assert(implementation_exists<ComponentInterface<ComponentType>>, "Component interface does not exist");

    const TypeID type_id = TypeID::ForType<ComponentType>();

    return static_cast<ComponentInterface<ComponentType> *>(GetComponentInterface(type_id));
}

} // namespace hyperion::v2

#endif