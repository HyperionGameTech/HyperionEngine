#include <scene/ecs/ComponentInterface.hpp>
#include <core/lib/FixedArray.hpp>

namespace hyperion::v2 {

struct ComponentInterfaceHolder
{
    static constexpr uint max_component_interfaces = 256;

    FixedArray<ComponentInterfaceBase *, max_component_interfaces> component_interfaces;
    uint num_component_interfaces = 0;

    void AddComponentInterface(ComponentInterfaceBase *component_interface)
    {
        AssertThrowMsg(num_component_interfaces < max_component_interfaces, "Maximum number of component interfaces reached");

        component_interfaces[num_component_interfaces++] = component_interface;
    }
};

static ComponentInterfaceHolder component_interface_holder { };

ComponentInterfaceBase::ComponentInterfaceBase(TypeID type_id, Array<ComponentProperty> &&properties)
    : m_type_id(type_id),
      m_properties(std::move(properties))
{
    component_interface_holder.AddComponentInterface(this);
}

ComponentProperty *ComponentInterfaceBase::GetProperty(Name name)
{
    for (auto &property : m_properties) {
        if (property.GetName() == name) {
            return &property;
        }
    }

    return nullptr;
}

const ComponentProperty *ComponentInterfaceBase::GetProperty(Name name) const
{
    for (auto &property : m_properties) {
        if (property.GetName() == name) {
            return &property;
        }
    }

    return nullptr;
}

ComponentInterfaceBase *GetComponentInterface(TypeID type_id)
{
    for (uint i = 0; i < component_interface_holder.num_component_interfaces; ++i) {
        if (component_interface_holder.component_interfaces[i]->GetTypeID() == type_id) {
            return component_interface_holder.component_interfaces[i];
        }
    }

    return nullptr;
}

} // namespace hyperion::v2