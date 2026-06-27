/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Class.hpp>

#include <Core/Threading/ThreadLocalStorage.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Util/ThreadId.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedClass.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/DotNETHost.hpp>
#endif

#define HYP_CLASS_REGISTRY_USE_TLS 1

namespace Hyperion {

CORE_API const Class* g_hypObjectBaseClass = nullptr;

#pragma region ClassRegistry

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS

using ThreadLocalCacheMap = TMap<TypeId, const Class*>;

static ThreadLocalCacheMap& GetDummyThreadLocalCache()
{
    static ThreadLocalCacheMap s_dummy;
    return s_dummy;
}

thread_local ThreadLocalCacheMap* t_cache;

static void InitThreadLocalCache()
{
    ThreadBase* thisThread = CurrentThreadObject();

    if (thisThread)
    {
        t_cache = thisThread->GetTLS().Allocate<ThreadLocalCacheMap>();

        if (t_cache)
        {
            new (t_cache) ThreadLocalCacheMap;

            thisThread->AddOnExitCallback([]()
                                          {
                                              t_cache->~ThreadLocalCacheMap();
                                          });

            return;
        }
    }

    // not mutated, just used as a fallback for searching from (won't return any results)
    t_cache = &GetDummyThreadLocalCache();
}

#endif

CORE_API bool ClassRegistry_IsInitialized()
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
#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (!typeId.IsDynamicType())
    {
        if (HYP_UNLIKELY(!t_cache))
        {
            InitThreadLocalCache();
        }

        auto it = t_cache->Find(typeId);
        if (it != t_cache->End())
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
    if (t_cache && t_cache != &GetDummyThreadLocalCache())
    {
        (*t_cache)[typeId] = it->second;
    }
#endif

    return it->second;
}

const Class* ClassRegistry::GetClass(StringHash typeName) const
{
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

const Class* ClassRegistry::GetClass(ANSIStringView typeName, bool ignoreCase) const
{
    if (typeName.Length() == 0)
    {
        return nullptr;
    }

    Mutex::Guard guard(m_mutex);

    ANSIString typeNameLower;
    StringHash typeNameHash = StringHash(typeName);

    const auto it = m_classesByTypeId.FindIf([typeName, ignoreCase, &typeNameLower, typeNameHash](auto&& item)
                                             {
                                                 if (ignoreCase)
                                                 {
                                                     // lazy lower-case conversion
                                                     if (typeNameLower.Empty())
                                                     {
                                                         typeNameLower = ANSIString(typeName).ToLower();
                                                     }

                                                     ANSIString itemNameLower = ANSIString(*item.second->GetName()).ToLower();

                                                     return itemNameLower == typeNameLower;
                                                 }

                                                 return item.second->GetName() == typeNameHash;
                                             });

    if (it == m_classesByTypeId.End())
    {
        auto dynamicIt = m_dynamicClasses.FindIf([typeName, ignoreCase, &typeNameLower](auto&& item)
                                                 {
                                                     if (ignoreCase)
                                                     {
                                                         // lazy lower-case conversion
                                                         if (typeNameLower.Empty())
                                                         {
                                                             typeNameLower = ANSIString(typeName).ToLower();
                                                         }

                                                         ANSIString itemNameLower = ANSIString(*item.second->GetName()).ToLower();

                                                         return itemNameLower == typeNameLower;
                                                     }

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
    const Class* cls = GetClass(typeId);

    if (!cls || !(cls->GetFlags() & ClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return cls;
}

const Class* ClassRegistry::GetEnum(StringHash typeName) const
{
    const Class* cls = GetClass(typeName);

    if (!cls || !(cls->GetFlags() & ClassFlags::ENUM_TYPE))
    {
        return nullptr;
    }

    return cls;
}

void ClassRegistry::Register(TypeId typeId, Class* cls, bool* outWasRegistered)
{
    if (typeId == TypeId::Void() || !cls)
    {
        if (outWasRegistered)
        {
            *outWasRegistered = false;
        }

        return;
    }

    Assert(typeId.IsDynamicType() == cls->IsDynamic());

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
            HYP_LOG(Object, Warning, "Attempted to register dynamic class with name {} but one already exists with that TypeId ({})!",
                    cls->GetName(), typeId.Value());

            if (outWasRegistered)
            {
                *outWasRegistered = false;
            }

            return;
        }

        HYP_LOG(Object, Verbose, "Registered dynamic class {}", cls->GetName());

        m_dynamicClasses.Set(typeId, cls);

        if (outWasRegistered)
        {
            *outWasRegistered = true;
        }

        return;
    }

    const auto it = m_classesByTypeId.Find(typeId);
    if (it != m_classesByTypeId.End())
    {
        if (outWasRegistered)
        {
            *outWasRegistered = false;
        }

        return;
    }

    m_classesByTypeId[typeId] = cls;

    if (outWasRegistered)
    {
        *outWasRegistered = true;
    }

    if (cls->GetStaticIndex() >= 0)
    {
        if (cls->GetStaticIndex() >= m_classesByStaticIndex.Size())
        {
            const size_t minSize = cls->GetStaticIndex() + 1;
            m_classesByStaticIndex.Resize((minSize + 15) & ~15); // grow by chunks of 16
        }

        m_classesByStaticIndex[cls->GetStaticIndex()] = cls;
    }

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (HYP_UNLIKELY(!t_cache))
    {
        InitThreadLocalCache();
    }

    if (t_cache && t_cache != &GetDummyThreadLocalCache())
    {
        (*t_cache)[typeId] = cls;
    }
#endif
}

bool ClassRegistry::Unregister(const Class* cls)
{
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

    HYP_LOG(Object, Verbose, "Unregister dynamic class {}", it->second->GetName());

    m_dynamicClasses.Erase(it);

    return true;
}

void ClassRegistry::ForEachClass(const ProcRef<IterationResult(const Class*)>& callback, bool includeDynamicClasses) const
{
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
    AssertOnThread(g_mainThread);

    HYP_CORE_ASSERT(!m_isInitialized);
    m_isInitialized = true;

    auto hypObjectBaseClassIt = m_classesByTypeId.FindIf([](auto&& item)
                                                         {
                                                             return item.second->GetName() == "ObjectBase"_sh;
                                                         });

    HYP_CORE_ASSERT(hypObjectBaseClassIt != m_classesByTypeId.End(), "ObjectBase class not registered");

    g_hypObjectBaseClass = hypObjectBaseClassIt->second;

    for (auto&& it : m_classesByTypeId)
    {
        HYP_CORE_ASSERT(it.second != nullptr);

        it.second->Initialize();
    }
}

#pragma endregion ClassRegistry

} // namespace Hyperion
