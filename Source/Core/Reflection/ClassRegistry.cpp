/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Class.hpp>

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

thread_local const Array<const Class*>* t_classLookupOverrides = nullptr;

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS

using ThreadLocalCacheMap = Map<TypeId, const Class*>;

static ThreadLocalCacheMap& GetDummyThreadLocalCache()
{
    static ThreadLocalCacheMap s_dummy;
    return s_dummy;
}

thread_local ThreadLocalCacheMap* t_cache;
thread_local uint32 t_cacheGeneration = 0;

static void InitThreadLocalCache()
{
    ThreadBase* thisThread = CurrentThreadObject();

    if (thisThread)
    {
        t_cache = new ThreadLocalCacheMap;

        if (t_cache)
        {
            thisThread->AddOnExitCallback(
                []()
                {
                    delete t_cache;
                    t_cache = nullptr;
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

#pragma region ClassLookupOverrideScope

ClassLookupOverrideScope::ClassLookupOverrideScope(const Array<const Class*>& classes)
    : m_previousClasses(t_classLookupOverrides)
{
    t_classLookupOverrides = &classes;
}

ClassLookupOverrideScope::~ClassLookupOverrideScope()
{
    t_classLookupOverrides = m_previousClasses;
}

const Class* ClassLookupOverrideScope::Find(TypeId typeId)
{
    if (HYP_LIKELY(t_classLookupOverrides == nullptr))
    {
        return nullptr;
    }

    for (const Class* cls : *t_classLookupOverrides)
    {
        if (cls->GetTypeId() == typeId)
        {
            return cls;
        }
    }

    return nullptr;
}

#pragma endregion ClassLookupOverrideScope

#pragma region ClassRegistry

ClassRegistry& ClassRegistry::GetInstance()
{
    static ClassRegistry s_instance;
    return s_instance;
}

ClassRegistry::ClassRegistry()
    : m_isInitialized(false),
      m_cacheGeneration(0)
{
}

ClassRegistry::~ClassRegistry()
{
}

const Class* ClassRegistry::GetClass(TypeId typeId) const
{
    if (const Class* overrideClass = ClassLookupOverrideScope::Find(typeId))
    {
        return overrideClass;
    }

#if defined(HYP_CLASS_REGISTRY_USE_TLS) && HYP_CLASS_REGISTRY_USE_TLS
    if (!typeId.IsDynamicType())
    {
        if (HYP_UNLIKELY(!t_cache))
        {
            InitThreadLocalCache();
        }

        if (const uint32 generation = m_cacheGeneration.Get(MemoryOrder::ACQUIRE); HYP_UNLIKELY(t_cacheGeneration != generation))
        {
            if (t_cache != &GetDummyThreadLocalCache())
            {
                t_cache->Clear();
            }

            t_cacheGeneration = generation;
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

const Class* ClassRegistry::GetClassByStaticIndex(int staticIndex) const
{
    if (staticIndex < 0)
    {
        return nullptr;
    }

    if (size_t(staticIndex) >= m_classesByStaticIndex.Size())
    {
        return nullptr;
    }

    return m_classesByStaticIndex[size_t(staticIndex)];
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
        if (m_isInitialized && m_deferInitializationDepth != 0)
        {
            m_classesPendingInitialization.PushBack(cls);

            m_mutex.Unlock();

            return;
        }

        m_mutex.Unlock();

        if (m_isInitialized)
        {
            cls->Initialize();
        }
    });

    if (cls->IsDynamic())
    {
        // a newer definition of the same type (e.g from a reloaded script assembly) supersedes the old one,
        // which stays alive for its existing instances until they release it
        if (auto it = m_dynamicClasses.Find(typeId); it != m_dynamicClasses.End() && it->second != cls)
        {
            HYP_LOG(Object, Verbose, "Dynamic class {} supersedes previous definition", cls->GetName());
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

    // only a module loaded after startup can register a static class now, and such a module may be unloaded again
    if (m_isInitialized)
    {
        cls->m_isModuleClass = true;
    }

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

Array<const Class*> ClassRegistry::UnregisterClassesInRange(UIntPtr begin, UIntPtr end)
{
    Array<const Class*> classes;

    Mutex::Guard guard(m_mutex);

    for (auto&& it : m_classesByTypeId)
    {
        const UIntPtr address = reinterpret_cast<UIntPtr>(it.second);

        if (address >= begin && address < end)
        {
            classes.PushBack(it.second);
        }
    }

    for (const Class* cls : classes)
    {
        HYP_LOG(Object, Verbose, "Unregister class {}", cls->GetName());

        if (cls->GetStaticIndex() >= 0 && size_t(cls->GetStaticIndex()) < m_classesByStaticIndex.Size())
        {
            m_classesByStaticIndex[cls->GetStaticIndex()] = nullptr;
        }

        m_classesByTypeId.Erase(cls->GetTypeId());
    }

    if (classes.Any())
    {
        m_cacheGeneration.Increment(1, MemoryOrder::RELEASE);
    }

    return classes;
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

void ClassRegistry::BeginDeferredInitialization()
{
    Mutex::Guard guard(m_mutex);

    m_deferInitializationDepth++;
}

void ClassRegistry::EndDeferredInitialization()
{
    Array<Class*> classes;

    {
        Mutex::Guard guard(m_mutex);

        HYP_CORE_ASSERT(m_deferInitializationDepth != 0);

        if (--m_deferInitializationDepth != 0)
        {
            return;
        }

        classes = std::move(m_classesPendingInitialization);
    }

    for (Class* cls : classes)
    {
        cls->Initialize();
    }
}

#pragma endregion ClassRegistry

} // namespace Hyperion
