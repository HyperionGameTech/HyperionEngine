/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/reflection/ObjectPool.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <Framework/EngineMemory.hpp>
#endif // HYPERION_ENGINE

namespace Hyperion {

CORE_API Pool* g_objectPool = nullptr;

CORE_API void ReleaseObject(ObjectHeader* header)
{
    AssertDebug(header != nullptr);

    const Class* cls = header->cls;
    AssertDebug(cls != nullptr);

    ObjectContainerBase* container = cls->GetObjectContainer();
    AssertDebug(container != nullptr, "Class has no ObjectContainer");

    container->Release(header);

    if (cls->IsDynamic())
    {
        AssertDebug(cls->IsClassType());

        const_cast<DynamicClassInstance*>(static_cast<const DynamicClassInstance*>(cls))->Release();
    }
}

#pragma region ObjectContainerMap

ObjectContainerMap& GetObjectContainerMap()
{
    static ObjectContainerMap s_objectContainerMap;

    return s_objectContainerMap;
}

ObjectContainerMap::~ObjectContainerMap()
{
    Shutdown();
}

void ObjectContainerMap::Shutdown()
{
    for (auto& it : m_map)
    {
        if (it.second != nullptr)
        {
            delete it.second;
            it.second = nullptr;
        }
    }

    m_map.Clear();
}

ObjectContainerBase& ObjectContainerMap::GetOrCreate(TypeId typeId, const Class* cls, ObjectContainerBase* (*createFn)(const Class* cls))
{
    AssertDebug(cls != nullptr);
    AssertDebug(cls->GetTypeId() != TypeId::Void());
    AssertDebug(cls->GetSize() != 0 && cls->GetAlignment() != 0);

    Mutex::Guard guard(m_mutex);

    auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it != m_map.End())
    {
        if (it->second == nullptr)
        {
            it->second = createFn(cls);

            AssertDebug(it->second != nullptr);
        }

        return *it->second;
    }

    ObjectContainerBase* container = createFn(cls);
    AssertDebug(container != nullptr);

    container->m_typeId = typeId;
    container->m_class = cls;

    if (ClassRegistry::GetInstance().IsInitialized())
    {
        // ClassRegistry already initialized; need to call Initialize() ourselves
        container->Initialize();
    }

    return *m_map.EmplaceBack(typeId, container).second;
}

ObjectContainerBase& ObjectContainerMap::Get(TypeId typeId)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it == m_map.End())
    {
        HYP_FAIL("No object container for Class: {}", LookupTypeName(typeId));
    }

    AssertDebug(it->second != nullptr);

    return *it->second;
}

ObjectContainerBase* ObjectContainerMap::TryGet(TypeId typeId)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it == m_map.End())
    {
        return nullptr;
    }

    return it->second;
}

#pragma endregion ObjectContainerMap

#pragma region ObjectContainerBase

ObjectContainerBase::ObjectContainerBase(TypeId typeId, const Class* cls)
    : m_typeId(typeId),
      m_class(cls),
      m_pool(nullptr)
{
    AssertDebug(typeId != TypeId::Void());
}

ObjectContainerBase::~ObjectContainerBase()
{
#if HYP_DEBUG_MODE
    if (m_pool != nullptr)
    {
        TLockGuard<AtomicFlag> guard;
        LockIfNeeded(guard, PF_WRITER | PF_ALLOCATE);

        for (ObjectHeader* header : m_headers)
        {
            AssertDebug(header != nullptr);

            if (!header)
                continue;

            int32 refCount = header->GetRefCountStrong();

            if (refCount > 0)
            {
                //HYP_LOG(Core, Warning, "Object {} still has {} strong references during ObjectContainer destruction",
                //    ObjIdBase(header->cls->GetTypeId(), header->index + 1),
                //    refCount);
            }
        }
    }
#endif
}

void ObjectContainerBase::LockIfNeeded(TLockGuard<AtomicFlag>& outGuard, int flags)
{
    AssertDebug(m_pool != nullptr);

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    if (m_pool == g_objectPool || (m_pool->GetFlags() & PF_THREAD_SAFE))
    {
#endif
        outGuard.Reset(m_atomicFlag);

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
        return;
    }
#endif
}

#pragma endregion ObjectContainerBase

} // namespace Hyperion
