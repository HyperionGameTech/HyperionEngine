/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypEnum.hpp>

#include <core/threading/ThreadID.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>

namespace hyperion {

#pragma region HypClassRegistry

HypClassRegistry& HypClassRegistry::GetInstance()
{
    static HypClassRegistry instance;

    return instance;
}

HypClassRegistry::HypClassRegistry()
    : m_is_initialized(false)
{
}

HypClassRegistry::~HypClassRegistry()
{
}

const HypClass* HypClassRegistry::GetClass(TypeID type_id) const
{
    AssertThrowMsg(m_is_initialized, "Cannot use GetClass() - HypClassRegistry instance not yet initialized");

    if (type_id.IsDynamicType())
    {
        Mutex::Guard guard(m_dynamic_classes_mutex);

        auto dynamic_it = m_dynamic_classes.Find(type_id);

        if (dynamic_it != m_dynamic_classes.End())
        {
            return dynamic_it->second;
        }

        return nullptr;
    }

    const auto it = m_registered_classes.Find(type_id);

    if (it == m_registered_classes.End())
    {
        return nullptr;
    }

    return it->second;
}

const HypClass* HypClassRegistry::GetClass(WeakName type_name) const
{
    AssertThrowMsg(m_is_initialized, "Cannot use GetClass() - HypClassRegistry instance not yet initialized");

    const auto it = m_registered_classes.FindIf([type_name](const Pair<TypeID, HypClass*>& item)
        {
            return item.second->GetName() == type_name;
        });

    if (it == m_registered_classes.End())
    {
        Mutex::Guard guard(m_dynamic_classes_mutex);

        auto dynamic_it = m_dynamic_classes.FindIf([type_name](const Pair<TypeID, HypClass*>& item)
            {
                return item.second->GetName() == type_name;
            });

        if (dynamic_it != m_dynamic_classes.End())
        {
            return dynamic_it->second;
        }

        return nullptr;
    }

    return it->second;
}

const HypEnum* HypClassRegistry::GetEnum(TypeID type_id) const
{
    const HypClass* hyp_class = GetClass(type_id);

    if (!hyp_class || !(hyp_class->GetFlags() & HypClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return static_cast<const HypEnum*>(hyp_class);
}

const HypEnum* HypClassRegistry::GetEnum(WeakName type_name) const
{
    const HypClass* hyp_class = GetClass(type_name);

    if (!hyp_class || !(hyp_class->GetFlags() & HypClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return static_cast<const HypEnum*>(hyp_class);
}

void HypClassRegistry::RegisterClass(TypeID type_id, HypClass* hyp_class)
{
    AssertThrow(hyp_class != nullptr);

    if (type_id.IsDynamicType())
    {
        AssertThrowMsg(hyp_class->IsDynamic(), "TypeID %u is dynamic but HypClass %s is not dynamic", type_id.Value(), hyp_class->GetName().LookupString());

        Mutex::Guard guard(m_dynamic_classes_mutex);

        HYP_LOG(Object, Info, "Register dynamic class {}", hyp_class->GetName());

        AssertThrowMsg(!m_dynamic_classes.Contains(type_id), "Dynamic class already registered for type: %s", *hyp_class->GetName());

        m_dynamic_classes.Set(type_id, hyp_class);

        return;
    }

    AssertThrowMsg(!m_is_initialized, "Cannot register class - HypClassRegistry instance already initialized");

    HYP_LOG(Object, Info, "Register class {}", hyp_class->GetName());

    const auto it = m_registered_classes.Find(type_id);
    AssertThrowMsg(it == m_registered_classes.End(), "Class already registered for type: %s", *hyp_class->GetName());

    m_registered_classes.Set(type_id, hyp_class);
}

void HypClassRegistry::UnregisterClass(const HypClass* hyp_class)
{
    AssertThrowMsg(hyp_class->GetTypeID().IsDynamicType(), "Cannot unregister class - must be a dynamic HypClass to unregister");

    Mutex::Guard guard(m_dynamic_classes_mutex);

    auto it = m_dynamic_classes.FindIf([hyp_class](const Pair<TypeID, HypClass*>& item)
        {
            return item.second == hyp_class;
        });

    if (it == m_dynamic_classes.End())
    {
        return;
    }

    HYP_LOG(Object, Info, "Unregister dynamic class {}", it->second->GetName());

    m_dynamic_classes.Erase(it);
}

void HypClassRegistry::ForEachClass(const ProcRef<IterationResult(const HypClass*)>& callback, bool include_dynamic_classes) const
{
    AssertThrowMsg(m_is_initialized, "Cannot use ForEachClass() - HypClassRegistry instance not yet initialized");

    for (const Pair<TypeID, HypClass*>& it : m_registered_classes)
    {
        if (callback(it.second) == IterationResult::STOP)
        {
            return;
        }
    }

    if (!include_dynamic_classes)
    {
        return;
    }

    Mutex::Guard guard(m_dynamic_classes_mutex);

    for (const Pair<TypeID, HypClass*>& it : m_dynamic_classes)
    {
        if (callback(it.second) == IterationResult::STOP)
        {
            return;
        }
    }
}

#if defined(HYP_DOTNET) && HYP_DOTNET

RC<dotnet::Class> HypClassRegistry::GetManagedClass(const HypClass* hyp_class) const
{
    if (!hyp_class)
    {
        return nullptr;
    }

    Mutex::Guard guard(m_managed_classes_mutex);

    auto it = m_managed_classes.FindAs(hyp_class);

    if (it == m_managed_classes.End())
    {
        return nullptr;
    }

    return it->second;
}

#endif

void HypClassRegistry::Initialize()
{
    Threads::AssertOnThread(g_main_thread);

    AssertThrow(!m_is_initialized);

    // Have to initialize here because HypClass::Initialize will call GetClass() for parent classes.
    m_is_initialized = true;

    for (const Pair<TypeID, HypClass*>& it : m_registered_classes)
    {
        it.second->Initialize();
    }
}

#pragma endregion HypClassRegistry

} // namespace hyperion