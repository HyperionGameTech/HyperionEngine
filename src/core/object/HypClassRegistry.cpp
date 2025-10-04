/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>

#include <core/threading/ThreadLocalStorage.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/ThreadId.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#ifdef HYP_DOTNET
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>
#endif

#define HYP_CLASS_REGISTRY_USE_TLS 1

namespace hyperion {

HYP_API const HypClass* g_hypObjectBaseClass = nullptr;

#pragma region HypClassRegistry

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS

using ThreadLocalCacheMap = HashMap<TypeId, const HypClass*>;

static ThreadLocalCacheMap& GetDummyThreadLocalCache()
{
    static ThreadLocalCacheMap s_dummy;
    return s_dummy;
}

thread_local ThreadLocalCacheMap* g_pThreadLocalCache;

static void InitThreadLocalCache()
{
    ThreadBase* thisThread = Threads::CurrentThreadObject();

    if (thisThread)
    {
        g_pThreadLocalCache = (ThreadLocalCacheMap*)thisThread->GetTLS().Alloc(sizeof(ThreadLocalCacheMap), alignof(ThreadLocalCacheMap));

        if (g_pThreadLocalCache)
        {
            new (g_pThreadLocalCache) ThreadLocalCacheMap;

            thisThread->AtExit([]()
                {
                    g_pThreadLocalCache->~ThreadLocalCacheMap();
                });

            return;
        }
    }

    // not mutated, just used as a fallback for searching from (won't return any results)
    g_pThreadLocalCache = &GetDummyThreadLocalCache();
}

#endif

HypClassRegistry& HypClassRegistry::GetInstance()
{
    static HypClassRegistry s_instance;
    return s_instance;
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
    HYP_SCOPE;

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (!typeId.IsDynamicType())
    {
        if (HYP_UNLIKELY(!g_pThreadLocalCache))
        {
            InitThreadLocalCache();
        }

        auto it = g_pThreadLocalCache->Find(typeId);
        if (it != g_pThreadLocalCache->End())
        {
            return it->second;
        }
    }
#endif

    Mutex::Guard guard(m_mutex);

    if (typeId.IsDynamicType())
    {
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

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (g_pThreadLocalCache && g_pThreadLocalCache != &GetDummyThreadLocalCache())
    {
        (*g_pThreadLocalCache)[typeId] = it->second;
    }
#endif

    return it->second;
}

const HypClass* HypClassRegistry::GetClass(WeakName typeName) const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    const auto it = m_registeredClasses.FindIf([typeName](auto&& item)
        {
            return item.second->GetName() == typeName;
        });

    if (it == m_registeredClasses.End())
    {
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

const HypClass* HypClassRegistry::GetEnum(TypeId typeId) const
{
    HYP_SCOPE;

    const HypClass* hypClass = GetClass(typeId);

    if (!hypClass || !(hypClass->GetFlags() & HypClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return hypClass;
}

const HypClass* HypClassRegistry::GetEnum(WeakName typeName) const
{
    HYP_SCOPE;

    const HypClass* hypClass = GetClass(typeName);

    if (!hypClass || !(hypClass->GetFlags() & HypClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return hypClass;
}

void HypClassRegistry::RegisterClass(TypeId typeId, HypClass* hypClass)
{
    HYP_SCOPE;

    if (typeId == TypeId::Void() || !hypClass)
    {
        return;
    }

    HYP_CORE_ASSERT(typeId.IsDynamicType() == hypClass->IsDynamic());

    m_mutex.Lock();

    HYP_DEFER({
        m_mutex.Unlock();

        if (m_isInitialized)
        {
            hypClass->Initialize();
        }
    });

    if (hypClass->IsDynamic())
    {
        if (m_dynamicClasses.Contains(typeId))
        {
            return;
        }

        m_dynamicClasses.Set(typeId, hypClass);

        return;
    }

    const auto it = m_registeredClasses.Find(typeId);
    if (it != m_registeredClasses.End())
    {
        return;
    }

    m_registeredClasses.Set(typeId, hypClass);

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (HYP_UNLIKELY(!g_pThreadLocalCache))
    {
        InitThreadLocalCache();
    }

    if (g_pThreadLocalCache && g_pThreadLocalCache != &GetDummyThreadLocalCache())
    {
        (*g_pThreadLocalCache)[typeId] = hypClass;
    }
#endif

    if (m_isInitialized)
    {
        hypClass->Initialize();
    }
}

bool HypClassRegistry::UnregisterClass(const HypClass* hypClass)
{
    HYP_SCOPE;

    HYP_CORE_ASSERT(hypClass->IsDynamic(), "Cannot unregister class - must be a dynamic HypClass to unregister");

    Mutex::Guard guard(m_mutex);

    auto it = m_dynamicClasses.FindIf([hypClass](auto&& item)
        {
            return item.second == hypClass;
        });

    if (it == m_dynamicClasses.End())
    {
        return false;
    }

    HYP_LOG(Object, Debug, "Unregister dynamic class {}", it->second->GetName());

    m_dynamicClasses.Erase(it);

    return true;
}

void HypClassRegistry::ForEachClass(const ProcRef<IterationResult(const HypClass*)>& callback, bool includeDynamicClasses) const
{
    HYP_SCOPE;

    Array<const HypClass*> classes;
    classes.Reserve(m_registeredClasses.Size() + (includeDynamicClasses ? m_dynamicClasses.Size() : 0));

    {
        Mutex::Guard guard(m_mutex);

        for (auto&& it : m_registeredClasses)
        {
            classes.PushBack(it.second);
        }

        if (includeDynamicClasses)
        {
            for (auto&& it : m_dynamicClasses)
            {
                classes.PushBack(it.second);
            }
        }
    }

    for (const HypClass* hypClass : classes)
    {
        if (callback(hypClass) == IterationResult::STOP)
        {
            return;
        }
    }
}

void HypClassRegistry::Initialize()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    HYP_CORE_ASSERT(!m_isInitialized);
    m_isInitialized = true;

    auto hypObjectBaseClassIt = m_registeredClasses.FindIf([](auto&& item)
        {
            return item.second->GetName() == "HypObjectBase";
        });

    HYP_CORE_ASSERT(hypObjectBaseClassIt != m_registeredClasses.End(), "HypObjectBase class not registered");

    g_hypObjectBaseClass = hypObjectBaseClassIt->second;

    for (auto&& it : m_registeredClasses)
    {
        it.second->Initialize();
    }
}

#pragma endregion HypClassRegistry

} // namespace hyperion
