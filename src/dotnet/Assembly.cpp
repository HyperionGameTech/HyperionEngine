/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Class.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>

namespace hyperion {

namespace dotnet {

Assembly::Assembly()
    : Assembly(AssemblyFlags::NONE)
{
}

Assembly::Assembly(EnumFlags<AssemblyFlags> flags)
    : m_flags(flags),
      m_guid { 0, 0 },
      m_invokeGetterFptr(nullptr),
      m_invokeSetterFptr(nullptr)
{
}

Assembly::~Assembly()
{
    if (!Unload())
    {
        HYP_LOG(DotNET, Warning, "Failed to unload assembly");
    }
}

bool Assembly::Unload()
{
    if (!IsLoaded())
    {
        return true;
    }

    for (const auto& it : m_classObjects)
    {
        const RC<Class>& classObject = it.second;

        if (!classObject)
        {
            continue;
        }

        if (const HypClass* hypClass = classObject->GetHypClass())
        {
            Assert(hypClass->GetManagedClass() == classObject, "HypClass '{}' does not match the expected managed class '{}'", *hypClass->GetName(), *classObject->GetName());

            hypClass->SetManagedClass(nullptr);
        }
    }

    return DotNetSystem::GetInstance().UnloadAssembly(m_guid);
}

RC<Class> Assembly::NewClass(const HypClass* hypClass, int32 typeHash, const char* typeName, uint32 typeSize, TypeId typeId, Class* parentClass, uint32 flags)
{
    auto it = m_classObjects.Find(typeHash);

    if (it != m_classObjects.End())
    {
        HYP_LOG(DotNET, Warning, "Class {} (type hash: {}) already exists in assembly with GUID {}!", typeName, typeHash, m_guid);

        return it->second;
    }

    it = m_classObjects.Insert(typeHash, MakeRefCountedPtr<Class>(WeakRefCountedPtrFromThis(), typeName, typeSize, typeId, hypClass, parentClass, EnumFlags<ManagedClassFlags>(flags))).first;

    if (hypClass != nullptr)
    {
        hypClass->SetManagedClass(it->second);
    }

    return it->second;
}

RC<Class> Assembly::FindClassByName(const char* typeName)
{
    for (auto& pair : m_classObjects)
    {
        if (pair.second->GetName() == typeName)
        {
            return pair.second;
        }
    }

    return nullptr;
}

RC<Class> Assembly::FindClassByTypeHash(int32 typeHash)
{
    auto it = m_classObjects.Find(typeHash);

    if (it != m_classObjects.End())
    {
        return it->second;
    }

    return nullptr;
}

} // namespace dotnet
} // namespace hyperion
