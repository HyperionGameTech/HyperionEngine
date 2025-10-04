/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/utilities/TypeInfo.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/object/HypClass.hpp>

namespace hyperion {
namespace utilities {

#pragma region Cache

struct TypeAttributeCacheKey
{
    TypeId typeId;
    SizeType size;
    SizeType alignment;

    HYP_FORCE_INLINE bool operator==(const TypeAttributeCacheKey& other) const
    {
        return typeId == other.typeId
            && size == other.size
            && alignment == other.alignment;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(typeId);
        hc.Add(size);
        hc.Add(alignment);

        return hc;
    }
};

using TypeAttributeCache = HashMap<TypeAttributeCacheKey, TypeInfo*>;

static bool g_typeInfoSystemInitialized = false;
static TypeAttributeCache* g_typeAttributeCache = nullptr;
static Pool* g_typeAttributePool = nullptr;

static Mutex& GetTypeAttributeCacheMutex()
{
    static Mutex s_typeAttributeCacheMutex;
    return s_typeAttributeCacheMutex;
}

static HashMap<TypeAttributeCacheKey, TypeInfo*>& GetTypeAttributeCache()
{
    static struct Initializer
    {
        Initializer()
        {
            g_typeAttributeCache = new TypeAttributeCache();
        }
    } s_initializer;

    return *g_typeAttributeCache;
}

static Pool& GetTypeAttributePool()
{
    static struct Initializer
    {
        Initializer()
        {
            g_typeAttributePool = new Pool(1024 * 1024); // 1 MB blocks
        }
    } s_initializer;

    return *g_typeAttributePool;
}

HYP_API TypeInfo* TypeInfo_Alloc(
    TypeId typeId, SizeType typeSize, SizeType typeAlignment,
    Mutex::Guard* outPGuard)
{
    const TypeAttributeCacheKey cacheKey { typeId, typeSize, typeAlignment };

    if (outPGuard) // otherwise assumed to be called from a context where the mutex is already held
    {
        new (outPGuard) Mutex::Guard(GetTypeAttributeCacheMutex());
    }

    TypeAttributeCache& typeAttributeCache = g_typeInfoSystemInitialized ? *g_typeAttributeCache : GetTypeAttributeCache();

    const auto it = typeAttributeCache.Find(cacheKey);
    if (it != typeAttributeCache.End())
    {
        return it->second;
    }

    TypeInfo* pTypeInfo = PoolAlloc<TypeInfo>(GetTypeAttributePool());
    HYP_CORE_ASSERT(pTypeInfo != nullptr);

    typeAttributeCache.Insert({ cacheKey, pTypeInfo });

    return pTypeInfo;
}

HYP_API TypeInfo* TypeInfo_FetchFromCache(TypeId typeId, SizeType size, SizeType alignment)
{
    const TypeAttributeCacheKey key { typeId, size, alignment };

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    TypeAttributeCache& typeAttributeCache = g_typeInfoSystemInitialized ? *g_typeAttributeCache : GetTypeAttributeCache();

    const auto it = typeAttributeCache.Find(key);
    if (it != typeAttributeCache.End())
    {
        return it->second;
    }

    return nullptr;
}

HYP_API void TypeInfo_Initialize()
{
    Threads::AssertOnThread(g_mainThread, "TypeInfo system must be initialized on the main thread");

    HYP_CORE_ASSERT(!g_typeInfoSystemInitialized, "TypeInfo system is already initialized");

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    // ensure the cache and pool are created
    (void)GetTypeAttributeCache();
    (void)GetTypeAttributePool();

    g_typeInfoSystemInitialized = true;
}

HYP_API void TypeInfo_Shutdown()
{
    Threads::AssertOnThread(g_mainThread, "TypeInfo system must be shutdown on the main thread");

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    HYP_CORE_ASSERT(g_typeInfoSystemInitialized, "TypeInfo system is not initialized");
    g_typeInfoSystemInitialized = false;

    for (auto& pair : *g_typeAttributeCache)
    {
        pair.second->~TypeInfo();
    }

    delete g_typeAttributeCache;
    g_typeAttributeCache = nullptr;

    delete g_typeAttributePool;
    g_typeAttributePool = nullptr;
}

#pragma endregion Cache

#pragma region TypeInfoEx

TypeInfoEx::TypeInfoEx(const TypeInfoEx& other)
    : data(other.data),
      dataType(other.dataType),
      next(other.next ? new TypeInfoEx(*other.next) : nullptr),
      handler(other.handler ? other.handler->Clone() : nullptr)
{
}

TypeInfoEx& TypeInfoEx::operator=(const TypeInfoEx& other)
{
    if (this != &other)
    {
        if (next)
        {
            delete next;
        }

        if (handler)
        {
            delete handler;
        }

        data = other.data;
        dataType = other.dataType;
        next = other.next ? new TypeInfoEx(*other.next) : nullptr;
        handler = other.handler ? other.handler->Clone() : nullptr;
    }

    return *this;
}

TypeInfoEx::TypeInfoEx(TypeInfoEx&& other) noexcept
    : data(other.data),
      dataType(other.dataType),
      next(other.next),
      handler(other.handler)
{
    other.data.typeInfo = nullptr;
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

        if (handler)
        {
            delete handler;
        }

        data = other.data;
        dataType = other.dataType;
        next = other.next;
        handler = other.handler;

        other.data.typeInfo = nullptr;
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

    if (handler)
    {
        delete handler;
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
    case DT_HYP_CLASS:
        hc.Add(data.hypClass);
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

HYP_API const TypeInfo& TypeInfo_Void()
{
    return TypeInfo::Void();
}

const TypeInfo& TypeInfo::Void()
{
    static TypeInfo s_voidTypeInfo;
    return s_voidTypeInfo;
}

const TypeInfo& TypeInfo::ForHypClass(const HypClass* hypClass)
{
    if (!hypClass)
    {
        return Void();
    }

    const TypeAttributeCacheKey key { hypClass->GetTypeId(), hypClass->GetSize(), hypClass->GetAlignment() };

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    const auto it = GetTypeAttributeCache().Find(key);
    if (it != GetTypeAttributeCache().End())
    {
        HYP_CORE_ASSERT(it->second != nullptr && it->second->GetHypClass() == hypClass);
        return *it->second;
    }

    // pass nullptr as we already hold the mutex
    TypeInfo* pTypeInfo = TypeInfo_Alloc(
        hypClass->GetTypeId(),
        hypClass->GetSize(),
        hypClass->GetAlignment(),
        nullptr);

    HYP_CORE_ASSERT(pTypeInfo != nullptr);

    new (pTypeInfo) TypeInfo();
    pTypeInfo->id = hypClass->GetTypeId();
    pTypeInfo->name = hypClass->GetName();
    pTypeInfo->size = hypClass->GetSize();
    pTypeInfo->alignment = hypClass->GetAlignment();
    pTypeInfo->flags = TypeAttributeFlags::HYP_CLASS;

    if (hypClass->IsClassType())
    {
        pTypeInfo->flags |= TypeAttributeFlags::CLASS_TYPE;
    }

    if (hypClass->IsStructType())
    {
        pTypeInfo->flags |= TypeAttributeFlags::STRUCT_TYPE;
    }

    if (hypClass->IsEnumType())
    {
        pTypeInfo->flags |= TypeAttributeFlags::ENUM_TYPE;
    }

    if (hypClass->IsPodType())
    {
        pTypeInfo->flags |= TypeAttributeFlags::POD_TYPE;
    }

    pTypeInfo->extendedInfo.data.hypClass = hypClass;
    pTypeInfo->extendedInfo.dataType = TypeInfoEx::DT_HYP_CLASS;

    return *pTypeInfo;
}

#pragma endregion TypeInfo

} // namespace utilities

} // namespace hyperion
