/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ComponentInterface.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/object/HypClass.hpp>

#include <core/utilities/TypeInfo.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_API bool ComponentInterface_CreateInstance(const HypClass* hypClass, HypData& outHypData)
{
    if (!hypClass || !hypClass->CanCreateInstance())
    {
        return false;
    }

    return hypClass->CreateInstance(outHypData);
}

#pragma region IComponentInterface

const HypClass* IComponentInterface::GetClass() const
{
    return GetTypeInfo().GetHypClass();
}

#pragma endregion IComponentInterface

#pragma region ComponentInterfaceRegistry

ComponentInterfaceRegistry& ComponentInterfaceRegistry::GetInstance()
{
    static ComponentInterfaceRegistry instance;

    return instance;
}

ComponentInterfaceRegistry::ComponentInterfaceRegistry()
    : m_isInitialized(false)
{
}

void ComponentInterfaceRegistry::Initialize()
{
    Assert(!m_isInitialized, "Component interface registry already initialized!");

    for (auto& it : m_factories)
    {
        m_interfaces.Set(it.first, it.second());
    }

    m_isInitialized = true;
}

void ComponentInterfaceRegistry::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

    m_interfaces.Clear();

    m_isInitialized = false;
}

void ComponentInterfaceRegistry::Register(TypeId typeId, UniquePtr<IComponentInterface> (*fptr)())
{
    m_factories.Set(typeId, fptr);
}

#pragma endregion ComponentInterfaceRegistry

} // namespace hyperion