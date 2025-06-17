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
      m_invoke_getter_fptr(nullptr),
      m_invoke_setter_fptr(nullptr)
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

    for (const auto& it : m_class_objects)
    {
        const RC<Class>& class_object = it.second;

        if (!class_object)
        {
            continue;
        }

        if (const HypClass* hyp_class = class_object->GetHypClass())
        {
            AssertThrowMsg(hyp_class->GetManagedClass() == class_object, "HypClass '%s' does not match the expected managed class '%s'", *hyp_class->GetName(), *class_object->GetName());

            hyp_class->SetManagedClass(nullptr);
        }
    }

    return DotNetSystem::GetInstance().UnloadAssembly(m_guid);
}

RC<Class> Assembly::NewClass(const HypClass* hyp_class, int32 type_hash, const char* type_name, uint32 type_size, TypeID type_id, Class* parent_class, uint32 flags)
{
    auto it = m_class_objects.Find(type_hash);

    if (it != m_class_objects.End())
    {
        HYP_LOG(DotNET, Warning, "Class {} (type hash: {}) already exists in assembly with GUID {}!", type_name, type_hash, m_guid);

        return it->second;
    }

    it = m_class_objects.Insert(type_hash, MakeRefCountedPtr<Class>(WeakRefCountedPtrFromThis(), type_name, type_size, type_id, hyp_class, parent_class, EnumFlags<ManagedClassFlags>(flags))).first;

    if (hyp_class != nullptr)
    {
        hyp_class->SetManagedClass(it->second);
    }

    return it->second;
}

RC<Class> Assembly::FindClassByName(const char* type_name)
{
    for (auto& pair : m_class_objects)
    {
        if (pair.second->GetName() == type_name)
        {
            return pair.second;
        }
    }

    return nullptr;
}

RC<Class> Assembly::FindClassByTypeHash(int32 type_hash)
{
    auto it = m_class_objects.Find(type_hash);

    if (it != m_class_objects.End())
    {
        return it->second;
    }

    return nullptr;
}

} // namespace dotnet
} // namespace hyperion