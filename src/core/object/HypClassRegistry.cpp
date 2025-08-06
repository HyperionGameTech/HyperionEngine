/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypEnum.hpp>

#include <core/threading/ThreadId.hpp>

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
    : m_isInitialized(false)
{
}

HypClassRegistry::~HypClassRegistry()
{
}

const HypClass* HypClassRegistry::GetClass(TypeId typeId) const
{
    HYP_CORE_ASSERT(m_isInitialized, "Cannot use GetClass() - HypClassRegistry instance not yet initialized");

    if (typeId.IsDynamicType())
    {
        Mutex::Guard guard(m_dynamicClassesMutex);

        auto dynamicIt = m_dynamicClasses.Find(typeId);

        if (dynamicIt != m_dynamicClasses.End())
        {
            return dynamicIt->second;
        }

        return nullptr;
    }

    const auto it = m_registeredClasses.Find(typeId);

    if (it == m_registeredClasses.End())
    {
        return nullptr;
    }

    return it->second;
}

const HypClass* HypClassRegistry::GetClass(WeakName typeName) const
{
    HYP_CORE_ASSERT(m_isInitialized, "Cannot use GetClass() - HypClassRegistry instance not yet initialized");

    const auto it = m_registeredClasses.FindIf([typeName](auto&& item)
        {
            return item.second->GetName() == typeName;
        });

    if (it == m_registeredClasses.End())
    {
        Mutex::Guard guard(m_dynamicClassesMutex);

        auto dynamicIt = m_dynamicClasses.FindIf([typeName](auto&& item)
            {
                return item.second->GetName() == typeName;
            });

        if (dynamicIt != m_dynamicClasses.End())
        {
            return dynamicIt->second;
        }

        return nullptr;
    }

    return it->second;
}

const HypEnum* HypClassRegistry::GetEnum(TypeId typeId) const
{
    const HypClass* hypClass = GetClass(typeId);

    if (!hypClass || !(hypClass->GetFlags() & HypClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return static_cast<const HypEnum*>(hypClass);
}

const HypEnum* HypClassRegistry::GetEnum(WeakName typeName) const
{
    const HypClass* hypClass = GetClass(typeName);

    if (!hypClass || !(hypClass->GetFlags() & HypClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return static_cast<const HypEnum*>(hypClass);
}

void HypClassRegistry::RegisterClass(TypeId typeId, HypClass* hypClass)
{
    HYP_CORE_ASSERT(hypClass != nullptr);

    if (typeId.IsDynamicType())
    {
        HYP_CORE_ASSERT(hypClass->IsDynamic(), "TypeId %u is dynamic but HypClass %s is not dynamic", typeId.Value(), hypClass->GetName().LookupString());

        Mutex::Guard guard(m_dynamicClassesMutex);

        HYP_LOG(Object, Debug, "Register dynamic class {}", hypClass->GetName());

        HYP_CORE_ASSERT(!m_dynamicClasses.Contains(typeId), "Dynamic class already registered for type: %s", *hypClass->GetName());

        m_dynamicClasses.Set(typeId, hypClass);

        return;
    }

    HYP_CORE_ASSERT(!m_isInitialized, "Cannot register class - HypClassRegistry instance already initialized");

    HYP_LOG(Object, Debug, "Register class {}", hypClass->GetName());

    const auto it = m_registeredClasses.Find(typeId);
    HYP_CORE_ASSERT(it == m_registeredClasses.End(), "Class already registered for type: %s", *hypClass->GetName());

    m_registeredClasses.Set(typeId, hypClass);
}

void HypClassRegistry::UnregisterClass(const HypClass* hypClass)
{
    HYP_CORE_ASSERT(hypClass->GetTypeId().IsDynamicType(), "Cannot unregister class - must be a dynamic HypClass to unregister");

    Mutex::Guard guard(m_dynamicClassesMutex);

    auto it = m_dynamicClasses.FindIf([hypClass](auto&& item)
        {
            return item.second == hypClass;
        });

    if (it == m_dynamicClasses.End())
    {
        return;
    }

    HYP_LOG(Object, Debug, "Unregister dynamic class {}", it->second->GetName());

    m_dynamicClasses.Erase(it);
}

void HypClassRegistry::ForEachClass(const ProcRef<IterationResult(const HypClass*)>& callback, bool includeDynamicClasses) const
{
    HYP_CORE_ASSERT(m_isInitialized, "Cannot use ForEachClass() - HypClassRegistry instance not yet initialized");

    for (auto&& it : m_registeredClasses)
    {
        if (callback(it.second) == IterationResult::STOP)
        {
            return;
        }
    }

    if (!includeDynamicClasses)
    {
        return;
    }

    Mutex::Guard guard(m_dynamicClassesMutex);

    for (auto&& it : m_dynamicClasses)
    {
        if (callback(it.second) == IterationResult::STOP)
        {
            return;
        }
    }
}

#if defined(HYP_DOTNET) && HYP_DOTNET

RC<dotnet::Class> HypClassRegistry::GetManagedClass(const HypClass* hypClass) const
{
    if (!hypClass)
    {
        return nullptr;
    }

    Mutex::Guard guard(m_managedClassesMutex);

    auto it = m_managedClasses.FindAs(hypClass);

    if (it == m_managedClasses.End())
    {
        return nullptr;
    }

    return it->second;
}

#endif

void HypClassRegistry::Initialize()
{
    Threads::AssertOnThread(g_mainThread);

    HYP_CORE_ASSERT(!m_isInitialized);

    // Have to initialize here because HypClass::Initialize will call GetClass() for parent classes.
    m_isInitialized = true;

    for (auto&& it : m_registeredClasses)
    {
        it.second->Initialize();
    }
}

#pragma endregion HypClassRegistry

} // namespace hyperion
