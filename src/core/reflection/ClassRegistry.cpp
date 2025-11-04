/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/Class.hpp>

#include <core/threading/ThreadLocalStorage.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/util/ThreadId.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#ifdef HYP_DOTNET
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>
#endif

#define HYP_CLASS_REGISTRY_USE_TLS 1

namespace hyperion {

HYP_API const Class* g_hypObjectBaseClass = nullptr;

#pragma region ClassRegistry

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS

using ThreadLocalCacheMap = HashMap<TypeId, const Class*>;

static ThreadLocalCacheMap& GetDummyThreadLocalCache()
{
    static ThreadLocalCacheMap s_dummy;
    return s_dummy;
}

thread_local ThreadLocalCacheMap* s_cache;

static void InitThreadLocalCache()
{
    ThreadBase* thisThread = Threads::CurrentThreadObject();

    if (thisThread)
    {
        s_cache = thisThread->GetTLS().Allocate<ThreadLocalCacheMap>();

        if (s_cache)
        {
            new (s_cache) ThreadLocalCacheMap;

            thisThread->AtExit([]()
                {
                    s_cache->~ThreadLocalCacheMap();
                });

            return;
        }
    }

    // not mutated, just used as a fallback for searching from (won't return any results)
    s_cache = &GetDummyThreadLocalCache();
}

#endif

HYP_API bool ClassRegistry_IsInitialized()
{
    return ClassRegistry::GetInstance().IsInitialized();
}

ClassRegistry& ClassRegistry::GetInstance()
{
    static ClassRegistry s_instance;
    return s_instance;
}

ClassRegistry::ClassRegistry()
    : m_isInitialized(false)
{
}

ClassRegistry::~ClassRegistry()
{
}

const Class* ClassRegistry::GetClass(TypeId typeId) const
{
    HYP_SCOPE;

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (!typeId.IsDynamicType())
    {
        if (HYP_UNLIKELY(!s_cache))
        {
            InitThreadLocalCache();
        }

        auto it = s_cache->Find(typeId);
        if (it != s_cache->End())
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

    const auto it = m_classesByTypeId.Find(typeId);

    if (it == m_classesByTypeId.End())
    {
        return nullptr;
    }

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (s_cache && s_cache != &GetDummyThreadLocalCache())
    {
        (*s_cache)[typeId] = it->second;
    }
#endif

    return it->second;
}

const Class* ClassRegistry::GetClass(WeakName typeName) const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    const auto it = m_classesByTypeId.FindIf([typeName](auto&& item)
        {
            return item.second->GetName() == typeName;
        });

    if (it == m_classesByTypeId.End())
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

const Class* ClassRegistry::GetEnum(TypeId typeId) const
{
    HYP_SCOPE;

    const Class* cls = GetClass(typeId);

    if (!cls || !(cls->GetFlags() & ClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return cls;
}

const Class* ClassRegistry::GetEnum(WeakName typeName) const
{
    HYP_SCOPE;

    const Class* cls = GetClass(typeName);

    if (!cls || !(cls->GetFlags() & ClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return cls;
}

void ClassRegistry::RegisterClass(TypeId typeId, Class* cls)
{
    HYP_SCOPE;

    if (typeId == TypeId::Void() || !cls)
    {
        return;
    }

    HYP_CORE_ASSERT(typeId.IsDynamicType() == cls->IsDynamic());

    m_mutex.Lock();

    HYP_DEFER({
        m_mutex.Unlock();

        if (m_isInitialized)
        {
            cls->Initialize();
        }
    });

    if (cls->IsDynamic())
    {
        if (m_dynamicClasses.Contains(typeId))
        {
            return;
        }

        m_dynamicClasses.Set(typeId, cls);

        return;
    }

    const auto it = m_classesByTypeId.Find(typeId);
    if (it != m_classesByTypeId.End())
    {
        return;
    }

    m_classesByTypeId[typeId] = cls;

    if (cls->GetStaticIndex() >= 0)
    {
        if (cls->GetStaticIndex() >= m_classesByStaticIndex.Size())
        {
            const SizeType minSize = cls->GetStaticIndex() + 1;
            m_classesByStaticIndex.Resize((minSize + 15) & ~15); // grow by chunks of 16
        }

        m_classesByStaticIndex[cls->GetStaticIndex()] = cls;
    }

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (HYP_UNLIKELY(!s_cache))
    {
        InitThreadLocalCache();
    }

    if (s_cache && s_cache != &GetDummyThreadLocalCache())
    {
        (*s_cache)[typeId] = cls;
    }
#endif

    if (m_isInitialized)
    {
        cls->Initialize();
    }
}

bool ClassRegistry::UnregisterClass(const Class* cls)
{
    HYP_SCOPE;

    HYP_CORE_ASSERT(cls->IsDynamic(), "Cannot unregister class - must be a dynamic Class to unregister");

    Mutex::Guard guard(m_mutex);

    auto it = m_dynamicClasses.FindIf([cls](auto&& item)
        {
            return item.second == cls;
        });

    if (it == m_dynamicClasses.End())
    {
        return false;
    }

    HYP_LOG(Object, Debug, "Unregister dynamic class {}", it->second->GetName());

    m_dynamicClasses.Erase(it);

    return true;
}

void ClassRegistry::ForEachClass(const ProcRef<IterationResult(const Class*)>& callback, bool includeDynamicClasses) const
{
    HYP_SCOPE;

    Array<const Class*> classes;
    classes.Reserve(m_classesByTypeId.Size() + (includeDynamicClasses ? m_dynamicClasses.Size() : 0));

    {
        Mutex::Guard guard(m_mutex);

        for (auto&& it : m_classesByTypeId)
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

    for (const Class* cls : classes)
    {
        if (callback(cls) == IterationResult::STOP)
        {
            return;
        }
    }
}

void ClassRegistry::Initialize()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    HYP_CORE_ASSERT(!m_isInitialized);
    m_isInitialized = true;

    auto hypObjectBaseClassIt = m_classesByTypeId.FindIf([](auto&& item)
        {
            return item.second->GetName() == "HypObjectBase";
        });

    HYP_CORE_ASSERT(hypObjectBaseClassIt != m_classesByTypeId.End(), "HypObjectBase class not registered");

    g_hypObjectBaseClass = hypObjectBaseClassIt->second;

    for (auto&& it : m_classesByTypeId)
    {
        it.second->Initialize();
    }
}

#pragma endregion ClassRegistry

} // namespace hyperion
