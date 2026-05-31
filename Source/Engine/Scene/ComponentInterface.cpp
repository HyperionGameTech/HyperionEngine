/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/ComponentInterface.hpp>

#include <Core/containers/FixedArray.hpp>

#include <Core/reflection/TypeInfo.hpp>

namespace Hyperion {

ENGINE_API bool ComponentInterface_CreateInstance(const Class* cls, BoxedValue& outBoxed)
{
    if (!cls || !cls->CanCreateInstance())
    {
        return false;
    }

    return cls->CreateInstance(outBoxed);
}

#pragma region IComponentInterface

const Class* IComponentInterface::GetClass() const
{
    return GetTypeInfo().GetClass();
}

#pragma endregion IComponentInterface

#pragma region ComponentInterfaceRegistry

ComponentInterfaceRegistry& ComponentInterfaceRegistry::GetInstance()
{
    static ComponentInterfaceRegistry s_instance;

    return s_instance;
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

} // namespace Hyperion
