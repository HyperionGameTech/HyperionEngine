/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Class.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

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

#ifdef HYP_DEBUG_MODE
    // Sanity check - ensure all owned classes, methods, objects etc will not be dangling

    for (auto &it : m_class_object_holder.m_class_objects) {
        if (!it.second) {
            continue;
        }
    }
#endif

    return DotNetSystem::GetInstance().UnloadAssembly(m_guid);   
}

#pragma region ClassHolder

ClassHolder::ClassHolder(Assembly *owner_assembly)
    : m_owner_assembly(owner_assembly),
      m_invoke_method_fptr(nullptr)
{
}

bool ClassHolder::CheckAssemblyLoaded() const
{
    if (m_owner_assembly) {
        return m_owner_assembly->IsLoaded();
    }

    return false;
}

Class *ClassHolder::NewClass(int32 type_hash, const char *type_name, Class *parent_class)
{
    auto it = m_class_objects.Find(type_hash);

    if (it != m_class_objects.End()) {
        HYP_LOG(DotNET, LogLevel::WARNING, "Class {} already exists in class holder!", type_name);

        return it->second.Get();
    }

    it = m_class_objects.Insert(type_hash, UniquePtr<Class>(new Class(this, type_name, parent_class))).first;

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