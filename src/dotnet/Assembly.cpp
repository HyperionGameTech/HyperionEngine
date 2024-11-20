/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Class.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClassRegistry.hpp>

namespace hyperion {
namespace dotnet {

Assembly::Assembly()
    : m_guid { 0, 0 },
      m_class_object_holder(this)
{
}

Assembly::~Assembly()
{
    if (!Unload()) {
        HYP_LOG(DotNET, LogLevel::WARNING, "Failed to unload assembly");
    }
}

bool Assembly::Unload()
{
    if (!IsLoaded()) {
        return true;
    }

    for (auto &it : m_class_object_holder.m_class_objects) {
        if (!it.second) {
            continue;
        }

        HypClassRegistry::GetInstance().UnregisterManagedClass(it.second.Get());
    }

    return DotNetSystem::GetInstance().UnloadAssembly(m_guid);   
}

#pragma region ClassHolder

ClassHolder::ClassHolder(Assembly *owner_assembly)
    : m_owner_assembly(owner_assembly),
      m_invoke_method_fptr(nullptr),
      m_invoke_getter_fptr(nullptr),
      m_invoke_setter_fptr(nullptr)
{
}

bool ClassHolder::CheckAssemblyLoaded() const
{
    if (m_owner_assembly) {
        return m_owner_assembly->IsLoaded();
    }

    return false;
}

Class *ClassHolder::NewClass(const HypClass *hyp_class, int32 type_hash, const char *type_name, uint32 type_size, TypeID type_id, Class *parent_class, uint32 flags)
{
    auto it = m_class_objects.Find(type_hash);

    if (it != m_class_objects.End()) {
        HYP_LOG(DotNET, LogLevel::WARNING, "Class {} already exists in class holder!", type_name);

        return it->second.Get();
    }

    it = m_class_objects.Insert(type_hash, MakeUnique<Class>(this, type_name, type_size, type_id, parent_class, EnumFlags<ManagedClassFlags>(flags))).first;

    if (hyp_class != nullptr) {
        HypClassRegistry::GetInstance().RegisterManagedClass(it->second.Get(), hyp_class);
    }

    return it->second.Get();
}

Class *ClassHolder::FindClassByName(const char *type_name)
{
    for (auto &pair : m_class_objects) {
        if (pair.second->GetName() == type_name) {
            return pair.second.Get();
        }
    }

    return nullptr;
}

Class *ClassHolder::FindClassByTypeHash(int32 type_hash)
{
    auto it = m_class_objects.Find(type_hash);

    if (it != m_class_objects.End()) {
        return it->second.Get();
    }

    return nullptr;
}

#pragma endregion ClassHolder

} // namespace dotnet
} // namespace hyperion