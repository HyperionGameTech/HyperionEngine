/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <DotNET/DotNETHost.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/ManagedClass.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Class.hpp>

namespace Hyperion {

namespace dotnet {

Assembly::Assembly(const ManagedGuid& guid)
    : Assembly(guid, AssemblyFlags::NONE)
{
}

Assembly::Assembly(const ManagedGuid& guid, EnumFlags<AssemblyFlags> flags)
    : m_flags(flags),
      m_guid(guid),
      m_invokeGetterFptr(nullptr),
      m_invokeSetterFptr(nullptr)
{
}

Assembly::~Assembly()
{
#ifdef HYP_DOTNET
    if (!Unload())
    {
        HYP_LOG(DotNET, Warning, "Failed to unload assembly");
    }
#endif
}

bool Assembly::Unload()
{
#ifdef HYP_DOTNET
    if (!IsLoaded())
    {
        return true;
    }

    for (const auto& it : m_classObjects)
    {
        const SharedPtr<ManagedClass>& classObject = it.second;

        if (!classObject)
        {
            continue;
        }

        if (const Class* cls = classObject->GetClass())
        {
            cls->SetManagedClass(nullptr);
        }
    }

    return DotNETHost::GetInstance().UnloadAssembly(m_guid);
#else
    return false;
#endif
}

SharedPtr<ManagedClass> Assembly::NewClass(const Class* cls, int32 typeHash, const char* typeName, uint32 typeSize, TypeId typeId, ManagedClass* parentClass, uint32 flags)
{
#ifdef HYP_DOTNET
    auto it = m_classObjects.Find(typeHash);

    if (it != m_classObjects.End())
    {
        HYP_LOG(DotNET, Warning, "Class {} (type hash: {}) already exists in assembly with GUID {}!", typeName, typeHash, m_guid);

        return it->second;
    }

    it = m_classObjects.Insert(typeHash, MakeShared<ManagedClass>(WeakThis(), typeName, typeSize, typeId, cls, parentClass, EnumFlags<ManagedClassFlags>(flags))).first;

    if (cls != nullptr)
    {
        cls->SetManagedClass(it->second);
    }

    return it->second;
#else
    return nullptr;
#endif
}

SharedPtr<ManagedClass> Assembly::FindClassByName(const char* typeName)
{
#ifdef HYP_DOTNET
    for (auto& pair : m_classObjects)
    {
        if (pair.second->GetName() == typeName)
        {
            return pair.second;
        }
    }

    return nullptr;
#else
    return nullptr;
#endif
}

SharedPtr<ManagedClass> Assembly::FindClassByTypeHash(int32 typeHash)
{
#ifdef HYP_DOTNET
    auto it = m_classObjects.Find(typeHash);

    if (it != m_classObjects.End())
    {
        return it->second;
    }

    return nullptr;
#else
    return nullptr;
#endif
}

} // namespace dotnet
} // namespace Hyperion
