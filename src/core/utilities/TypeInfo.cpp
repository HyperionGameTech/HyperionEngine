/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>

#include <core/utilities/TypeInfo.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/pool/Pool.hpp>

namespace hyperion {
namespace utilities {

#pragma region Cache

using TypeAttributeCache = HashMap<TypeId, TypeInfo*>;

static bool s_typeInfoSystemInitialized = false;
static TypeAttributeCache* s_typeAttributeCache = nullptr;
static Pool* s_typeAttributePool = nullptr;

static Mutex& GetTypeAttributeCacheMutex()
{
    static Mutex s_typeAttributeCacheMutex;
    return s_typeAttributeCacheMutex;
}

static HashMap<TypeId, TypeInfo*>& GetTypeAttributeCache()
{
    static struct Initializer
    {
        Initializer()
        {
            s_typeAttributeCache = new TypeAttributeCache();
        }
    } s_initializer;

    return *s_typeAttributeCache;
}

static Pool& GetTypeAttributePool()
{
    static struct Initializer
    {
        Initializer()
        {
            s_typeAttributePool = new Pool(16 * 1024); // 16 kib blocks (~292 TypeInfo per block)
        }
    } s_initializer;

    return *s_typeAttributePool;
}

HYP_API TypeInfo* TypeInfo_Alloc(
    TypeId typeId, uint16 typeSize, uint16 typeAlignment,
    Mutex::Guard* outPGuard)
{
    HYP_CORE_ASSERT(typeId != TypeId::Void(), "Cannot allocate TypeInfo for void type");

    if (outPGuard) // otherwise assumed to be called from a context where the mutex is already held
    {
        new (outPGuard) Mutex::Guard(GetTypeAttributeCacheMutex());
    }

    TypeAttributeCache& typeAttributeCache = s_typeInfoSystemInitialized ? *s_typeAttributeCache : GetTypeAttributeCache();

    const auto it = typeAttributeCache.Find(typeId);
    if (it != typeAttributeCache.End())
    {
        return it->second;
    }

    TypeInfo* pTypeInfo = PoolAlloc<TypeInfo>(GetTypeAttributePool());
    HYP_CORE_ASSERT(pTypeInfo != nullptr);

    typeAttributeCache.Insert({ typeId, pTypeInfo });

    return pTypeInfo;
}

HYP_API TypeInfo* TypeInfo_FetchFromCache(TypeId typeId, uint16 size, uint16 alignment)
{
    HYP_CORE_ASSERT(typeId != TypeId::Void(), "Cannot allocate TypeInfo for void type");

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    TypeAttributeCache& typeAttributeCache = s_typeInfoSystemInitialized ? *s_typeAttributeCache : GetTypeAttributeCache();

    const auto it = typeAttributeCache.Find(typeId);
    if (it != typeAttributeCache.End())
    {
        return it->second;
    }

    return nullptr;
}

HYP_API void TypeInfo_Initialize()
{
    Threads::AssertOnThread(g_mainThread, "TypeInfo system must be initialized on the main thread");

    HYP_CORE_ASSERT(!s_typeInfoSystemInitialized, "TypeInfo system is already initialized");

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    // ensure the cache and pool are created
    (void)GetTypeAttributeCache();
    (void)GetTypeAttributePool();

    s_typeInfoSystemInitialized = true;
}

HYP_API void TypeInfo_Shutdown()
{
    Threads::AssertOnThread(g_mainThread, "TypeInfo system must be shutdown on the main thread");

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    HYP_CORE_ASSERT(s_typeInfoSystemInitialized, "TypeInfo system is not initialized");
    s_typeInfoSystemInitialized = false;

    for (auto& pair : *s_typeAttributeCache)
    {
        pair.second->~TypeInfo();
    }

    delete s_typeAttributeCache;
    s_typeAttributeCache = nullptr;

    delete s_typeAttributePool;
    s_typeAttributePool = nullptr;
}

#pragma endregion Cache

#pragma region TypeInfoEx

TypeInfoEx::TypeInfoEx(const TypeInfoEx& other)
    : dataType(other.dataType),
      next(other.next ? new TypeInfoEx(*other.next) : nullptr),
      handler(other.handler ? other.handler->Clone() : nullptr)
{
    Memory::MemCpy(&data, &other.data, sizeof(data));
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

        Memory::MemCpy(&data, &other.data, sizeof(data));

        dataType = other.dataType;
        next = other.next ? new TypeInfoEx(*other.next) : nullptr;
        handler = other.handler ? other.handler->Clone() : nullptr;
    }

    return *this;
}

TypeInfoEx::TypeInfoEx(TypeInfoEx&& other) noexcept
    : dataType(other.dataType),
      next(other.next),
      handler(other.handler)
{
    Memory::MemCpy(&data, &other.data, sizeof(data));
    Memory::MemSet(&other.data, 0, sizeof(data));

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

        Memory::MemCpy(&data, &other.data, sizeof(data));
        Memory::MemSet(&other.data, 0, sizeof(data));

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

HYP_DISABLE_OPTIMIZATION;
const TypeInfo& TypeInfo::ForHypClass(const HypClass* hypClass)
{
    if (!hypClass)
    {
        return Void();
    }

    Mutex::Guard guard(GetTypeAttributeCacheMutex());

    const auto it = GetTypeAttributeCache().Find(hypClass->GetTypeId());
    if (it != GetTypeAttributeCache().End())
    {
        if (s_typeInfoSystemInitialized) // don't check during static initialization
        {
            HYP_CORE_ASSERT(it->second != nullptr && it->second->GetHypClass() == hypClass);
        }
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
    pTypeInfo->size = uint16(hypClass->GetSize());
    pTypeInfo->alignment = uint16(hypClass->GetAlignment());
    pTypeInfo->flags = TypeInfoFlags::NONE;

    HYP_CORE_ASSERT(pTypeInfo->name.IsValid());

    if (hypClass->IsClassType())
    {
        pTypeInfo->flags |= TypeInfoFlags::CLASS_TYPE;
    }

    if (hypClass->IsStructType())
    {
        pTypeInfo->flags |= TypeInfoFlags::STRUCT_TYPE;
    }

    if (hypClass->IsEnumType())
    {
        pTypeInfo->flags |= TypeInfoFlags::ENUM_TYPE;
    }

    if (hypClass->IsPodType())
    {
        pTypeInfo->flags |= TypeInfoFlags::POD_TYPE;
    }

    return *pTypeInfo;
}

HYP_ENABLE_OPTIMIZATION;

#pragma endregion TypeInfo

} // namespace utilities

} // namespace hyperion
