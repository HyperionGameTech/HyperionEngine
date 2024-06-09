/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/ComponentInterface.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

#pragma region ComponentInterfaceRegistry

ComponentInterfaceRegistry &ComponentInterfaceRegistry::GetInstance()
{
    static ComponentInterfaceRegistry instance;

    return instance;
}

ComponentInterfaceRegistry::ComponentInterfaceRegistry()
    : m_is_initialized(false)
{
}

void ComponentInterfaceRegistry::Initialize()
{
    AssertThrowMsg(!m_is_initialized, "Component interface registry already initialized!");

    HYP_LOG(ECS, LogLevel::DEBUG, "Initializing ComponentInterface registry with {} factories", m_factories.Size());

    for (auto &it : m_factories) {
        m_interfaces.Insert({ it.first, it.second() });
    }

    for (auto &it : m_interfaces) {
        it.second->Initialize();

        HYP_LOG(ECS, LogLevel::DEBUG, "Create new ComponentInterface with {} Properties", it.second->GetProperties().Size());
    }

    m_is_initialized = true;
}

void ComponentInterfaceRegistry::Shutdown()
{
    if (!m_is_initialized) {
        return;
    }

    m_interfaces.Clear();

    m_is_initialized = false;
}

void ComponentInterfaceRegistry::Register(TypeID component_type_id, UniquePtr< ComponentInterfaceBase >(*fptr)())
{
    m_factories.Insert({ component_type_id, fptr });
}

#pragma endregion ComponentInterfaceRegistry

#pragma region ComponentInterfaceBase

void ComponentInterfaceBase::Initialize()
{
    m_type_id = GetTypeID_Internal();
    m_type_name = GetTypeName_Internal();
    m_properties = GetProperties_Internal();
}

ComponentInterfaceBase::ComponentInterfaceBase()
    : m_component_container_factory(nullptr),
      m_type_id(TypeID::Void())
{
}

ComponentProperty *ComponentInterfaceBase::GetProperty(WeakName name)
{
    for (ComponentProperty &property : m_properties) {
        if (property.GetName() == name) {
            return &property;
        }
    }

    return nullptr;
}

const ComponentProperty *ComponentInterfaceBase::GetProperty(WeakName name) const
{
    for (auto &property : m_properties) {
        if (property.GetName() == name) {
            return &property;
        }
    }

    return nullptr;
}

#pragma endregion ComponentInterfaceBase

} // namespace hyperion