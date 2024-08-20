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
        m_interfaces.Set(it.first, it.second());
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

void ComponentInterfaceRegistry::Register(TypeID type_id, ANSIStringView type_name, ComponentInterface(*fptr)())
{
    HYP_LOG(ECS, LogLevel::INFO, "Register Component '{}', TypeID: {}", type_name, type_id.Value());

    m_factories.Set(type_id, fptr);
}

#pragma endregion ComponentInterfaceRegistry

} // namespace hyperion