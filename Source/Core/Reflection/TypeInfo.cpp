/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/Class.hpp>

#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/ThreadLocalStorage.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Util/ThreadId.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Memory/Pool/Pool.hpp>
#include <Core/Memory/Allocator/SlabAllocator.hpp>

//#define HYP_TYPE_INFO_USE_TLS 1

namespace Hyperion {
namespace utilities {

#pragma region Cache

using TypeInfoCache = TMap<TypeId, TypeInfo*>;

static bool s_typeInfoSystemInitialized = false;
static TypeInfoCache* s_typeInfoCache = nullptr;
static SlabAllocator* s_typeInfoAllocator = nullptr;

static Mutex& GetTypeInfoCacheMutex()
{
    static Mutex s_typeInfoCacheMutex;
    return s_typeInfoCacheMutex;
}

static TMap<TypeId, TypeInfo*>& GetTypeInfoCache()
{
    static struct Initializer
    {
        Initializer()
        {
            s_typeInfoCache = new TypeInfoCache();
        }
    } s_initializer;

    return *s_typeInfoCache;
}

static SlabAllocator& GetTypeInfoAllocator()
{
    static struct Initializer
    {
        Initializer()
        {
            s_typeInfoAllocator = new SlabAllocator(sizeof(TypeInfo), alignof(TypeInfo));
        }
    } s_initializer;

    return *s_typeInfoAllocator;
}

#if defined(HYP_TYPE_INFO_USE_TLS) && HYP_TYPE_INFO_USE_TLS

using ThreadLocalCacheMap = TMap<TypeId, TypeInfo*>;

static ThreadLocalCacheMap& GetDummyThreadLocalCache()
{
    static ThreadLocalCacheMap s_dummy;
    return s_dummy;
}

thread_local ThreadLocalCacheMap* s_typeInfoThreadLocalCache;

static void InitTypeInfoThreadLocalCache()
{
    ThreadBase* thisThread = CurrentThreadObject();

    if (thisThread)
    {
        s_typeInfoThreadLocalCache = thisThread->GetTLS().Allocate<ThreadLocalCacheMap>();

        if (s_typeInfoThreadLocalCache)
        {
            new (s_typeInfoThreadLocalCache) ThreadLocalCacheMap;

            thisThread->AddOnExitCallback([]()
                {
                    s_typeInfoThreadLocalCache->~ThreadLocalCacheMap();
                });

            return;
        }
    }

    s_typeInfoThreadLocalCache = &GetDummyThreadLocalCache();
}

#endif

CORE_API TypeInfo* TypeInfo_Alloc(
    TypeId typeId, uint16 typeSize, uint16 typeAlignment,
    Mutex::Guard* outPGuard)
{
    AssertDebug(typeId != TypeId::Void(), "Cannot allocate TypeInfo for void type");

    if (outPGuard) // otherwise assumed to be called from a context where the mutex is already held
    {
        new (outPGuard) Mutex::Guard(GetTypeInfoCacheMutex());
    }

    TypeInfoCache& typeAttributeCache = s_typeInfoSystemInitialized ? *s_typeInfoCache : GetTypeInfoCache();

    const auto it = typeAttributeCache.Find(typeId);
    if (it != typeAttributeCache.End())
    {
#if defined(HYP_TYPE_INFO_USE_TLS) && HYP_TYPE_INFO_USE_TLS
        if (HYP_UNLIKELY(!s_typeInfoThreadLocalCache))
        {
            InitTypeInfoThreadLocalCache();
        }

        if (s_typeInfoThreadLocalCache && s_typeInfoThreadLocalCache != &GetDummyThreadLocalCache())
        {
            (*s_typeInfoThreadLocalCache)[typeId] = it->second;
        }
#endif
        return it->second;
    }

    TypeInfo* pTypeInfo = (TypeInfo*)GetTypeInfoAllocator().Allocate();
    AssertDebug(pTypeInfo != nullptr);

    typeAttributeCache.Insert({ typeId, pTypeInfo });

#if defined(HYP_TYPE_INFO_USE_TLS) && HYP_TYPE_INFO_USE_TLS
    if (HYP_UNLIKELY(!s_typeInfoThreadLocalCache))
    {
        InitTypeInfoThreadLocalCache();
    }

    if (s_typeInfoThreadLocalCache && s_typeInfoThreadLocalCache != &GetDummyThreadLocalCache())
    {
        (*s_typeInfoThreadLocalCache)[typeId] = pTypeInfo;
    }
#endif

    return pTypeInfo;
}

CORE_API TypeInfo* TypeInfo_FetchFromCache(TypeId typeId, uint16 size, uint16 alignment)
{
    AssertDebug(typeId != TypeId::Void(), "Cannot allocate TypeInfo for void type");

#if defined(HYP_TYPE_INFO_USE_TLS) && HYP_TYPE_INFO_USE_TLS
    if (HYP_UNLIKELY(!s_typeInfoThreadLocalCache))
    {
        InitTypeInfoThreadLocalCache();
    }

    auto tlsIt = s_typeInfoThreadLocalCache->Find(typeId);
    if (tlsIt != s_typeInfoThreadLocalCache->End())
    {
        return tlsIt->second;
    }
#endif

    Mutex::Guard guard(GetTypeInfoCacheMutex());

    TypeInfoCache& typeAttributeCache = s_typeInfoSystemInitialized ? *s_typeInfoCache : GetTypeInfoCache();

    const auto it = typeAttributeCache.Find(typeId);
    if (it != typeAttributeCache.End())
    {
#if defined(HYP_TYPE_INFO_USE_TLS) && HYP_TYPE_INFO_USE_TLS
        if (s_typeInfoThreadLocalCache && s_typeInfoThreadLocalCache != &GetDummyThreadLocalCache())
        {
            (*s_typeInfoThreadLocalCache)[typeId] = it->second;
        }
#endif
        return it->second;
    }

    return nullptr;
}

CORE_API void TypeInfo_Initialize()
{
    AssertOnThread(g_mainThread, "TypeInfo system must be initialized on the main thread");

    AssertDebug(!s_typeInfoSystemInitialized, "TypeInfo system is already initialized");

    Mutex::Guard guard(GetTypeInfoCacheMutex());

    // ensure the cache and pool are created
    (void)GetTypeInfoCache();
    (void)GetTypeInfoAllocator();

    s_typeInfoSystemInitialized = true;
}

CORE_API void TypeInfo_Shutdown()
{
    AssertOnThread(g_mainThread, "TypeInfo system must be shutdown on the main thread");

    Mutex::Guard guard(GetTypeInfoCacheMutex());

    AssertDebug(s_typeInfoSystemInitialized, "TypeInfo system is not initialized");
    s_typeInfoSystemInitialized = false;

    for (auto& pair : *s_typeInfoCache)
    {
        pair.second->~TypeInfo();
    }

    delete s_typeInfoCache;
    s_typeInfoCache = nullptr;

    delete s_typeInfoAllocator;
    s_typeInfoAllocator = nullptr;
}

#pragma endregion Cache

#pragma region TypeInfoEx

TypeInfoEx::TypeInfoEx(const TypeInfoEx& other)
    : dataType(other.dataType),
      next(other.next ? new TypeInfoEx(*other.next) : nullptr),
      handler(other.handler ? other.handler : nullptr)
{
    Memory::Copy(&data, &other.data, sizeof(data));
}

TypeInfoEx& TypeInfoEx::operator=(const TypeInfoEx& other)
{
    if (this != &other)
    {
        if (next)
        {
            delete next;
        }

        Memory::Copy(&data, &other.data, sizeof(data));

        dataType = other.dataType;
        next = other.next ? new TypeInfoEx(*other.next) : nullptr;
        handler = other.handler ? other.handler : nullptr;
    }

    return *this;
}

TypeInfoEx::TypeInfoEx(TypeInfoEx&& other) noexcept
    : dataType(other.dataType),
      next(other.next),
      handler(other.handler)
{
    Memory::Copy(&data, &other.data, sizeof(data));
    Memory::Fill(&other.data, 0, sizeof(data));

    other.dataType = DT_NONE;
    other.next = nullptr;
    other.handler = nullptr;
}

TypeInfoEx& TypeInfoEx::operator=(TypeInfoEx&& other) noexcept
{
    if (this != &other)
    {
        if (next)
        {
            delete next;
        }

        Memory::Copy(&data, &other.data, sizeof(data));
        Memory::Fill(&other.data, 0, sizeof(data));

        dataType = other.dataType;
        next = other.next;
        handler = other.handler;

        other.dataType = DT_NONE;
        other.next = nullptr;
        other.handler = nullptr;
    }

    return *this;
}

TypeInfoEx::~TypeInfoEx()
{
    if (next)
    {
        delete next;
    }
}

HashCode TypeInfoEx::GetHashCode() const
{
    HashCode hc;

    hc.Add(static_cast<uint32>(dataType));

    switch (dataType)
    {
    case DT_TYPE_INFO:
        if (data.typeInfo)
        {
            hc.Add(data.typeInfo->GetHashCode());
        }
        break;
    default:
        break;
    }

    if (next)
    {
        hc.Add(next->GetHashCode());
    }

    return hc;
}

#pragma endregion TypeInfoEx

#pragma region TypeInfo

const TypeInfo& TypeInfo::Void()
{
    static TypeInfo s_voidTypeInfo;
    return s_voidTypeInfo;
}

const TypeInfo& TypeInfo::ForClass(const Class* cls)
{
    if (!cls)
    {
        return Void();
    }

    // get the one stored on the class if it exists
    if (const TypeInfo* pExisting = cls->GetTypeInfo())
    {
        return *pExisting;
    }

    // we don't want to cache dynamic classes as they can be destroyed at runtime
    AssertDebug(!cls->IsDynamic());

#if defined(HYP_TYPE_INFO_USE_TLS) && HYP_TYPE_INFO_USE_TLS
    if (HYP_UNLIKELY(!s_typeInfoThreadLocalCache))
    {
        InitTypeInfoThreadLocalCache();
    }

    auto tlsIt = s_typeInfoThreadLocalCache->Find(cls->GetTypeId());
    if (tlsIt != s_typeInfoThreadLocalCache->End())
    {
        if (s_typeInfoSystemInitialized) // don't check during static initialization
        {
            AssertDebug(tlsIt->second != nullptr && tlsIt->second->GetClass() == cls);
        }
        return *tlsIt->second;
    }
#endif

    // look in our cache
    Mutex::Guard guard(GetTypeInfoCacheMutex());

    const auto it = GetTypeInfoCache().Find(cls->GetTypeId());
    if (it != GetTypeInfoCache().End())
    {
        if (s_typeInfoSystemInitialized) // don't check during static initialization
        {
            AssertDebug(it->second != nullptr && it->second->GetClass() == cls);
        }

#if defined(HYP_TYPE_INFO_USE_TLS) && HYP_TYPE_INFO_USE_TLS
        if (s_typeInfoThreadLocalCache && s_typeInfoThreadLocalCache != &GetDummyThreadLocalCache())
        {
            (*s_typeInfoThreadLocalCache)[cls->GetTypeId()] = it->second;
        }
#endif
        return *it->second;
    }

    // pass nullptr as we already hold the mutex
    TypeInfo* pTypeInfo = TypeInfo_Alloc(
        cls->GetTypeId(),
        cls->GetSize(),
        cls->GetAlignment(),
        nullptr);

    AssertDebug(pTypeInfo != nullptr);

    new (pTypeInfo) TypeInfo();
    pTypeInfo->id = cls->GetTypeId();
    pTypeInfo->name = cls->GetName();
    pTypeInfo->size = uint16(cls->GetSize());
    pTypeInfo->alignment = uint16(cls->GetAlignment());
    pTypeInfo->flags = TypeInfoFlags::NONE;

    AssertDebug(pTypeInfo->name.IsValid());

    if (cls->IsClassType())
    {
        pTypeInfo->flags |= TypeInfoFlags::CLASS_TYPE;
    }

    if (cls->IsStructType())
    {
        pTypeInfo->flags |= TypeInfoFlags::STRUCT_TYPE;
    }

    if (cls->IsEnumType())
    {
        pTypeInfo->flags |= TypeInfoFlags::ENUM_TYPE;
    }

    if (cls->IsPodType())
    {
        pTypeInfo->flags |= TypeInfoFlags::POD_TYPE;
    }

    return *pTypeInfo;
}

TypeInfo* TypeInfo::ForDynamicClass(const Class* cls)
{
    if (!cls)
    {
        return nullptr;
    }

    if (const TypeInfo* pExisting = cls->GetTypeInfo())
    {
        return const_cast<TypeInfo*>(pExisting);
    }

    AssertDebug(cls->IsDynamic());

    TypeInfo* pTypeInfo = new TypeInfo();
    pTypeInfo->id = cls->GetTypeId();
    pTypeInfo->name = cls->GetName();
    pTypeInfo->size = uint16(cls->GetSize());
    pTypeInfo->alignment = uint16(cls->GetAlignment());
    pTypeInfo->flags = TypeInfoFlags::NONE;

    AssertDebug(pTypeInfo->name.IsValid());

    if (cls->IsClassType())
    {
        pTypeInfo->flags |= TypeInfoFlags::CLASS_TYPE;
    }

    if (cls->IsStructType())
    {
        pTypeInfo->flags |= TypeInfoFlags::STRUCT_TYPE;
    }

    if (cls->IsEnumType())
    {
        pTypeInfo->flags |= TypeInfoFlags::ENUM_TYPE;
    }

    if (cls->IsPodType())
    {
        pTypeInfo->flags |= TypeInfoFlags::POD_TYPE;
    }

    return pTypeInfo;
}

#pragma endregion TypeInfo

} // namespace utilities

} // namespace Hyperion
