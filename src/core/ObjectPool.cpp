/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/ObjectPool.hpp>

namespace hyperion {

ObjectPool::ObjectContainerHolder &ObjectPool::GetObjectContainerHolder()
{
    static ObjectPool::ObjectContainerHolder holder { };

    return holder;
}

UniquePtr<ObjectContainerBase> *ObjectPool::ObjectContainerHolder::AllotObjectContainer(TypeID type_id)
{
    // Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    auto it = object_container_map.map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    // Already allocated
    if (it != object_container_map.map.End()) {
        return &it->second;
    }

    object_container_map.map.PushBack({
        type_id,
        nullptr
    });

    return &object_container_map.map.Back().second;
}

ObjectContainerBase &ObjectPool::ObjectContainerHolder::GetObjectContainer(TypeID type_id)
{
    const auto it = object_container_map.map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    if (it == object_container_map.map.End()) {
        HYP_FAIL("No object container for TypeID: %u", type_id.Value());
    }

    AssertThrow(it->second != nullptr);

    return *it->second;
}

ObjectContainerBase *ObjectPool::ObjectContainerHolder::TryGetObjectContainer(TypeID type_id)
{
    const auto it = object_container_map.map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    if (it == object_container_map.map.End()) {
        return nullptr;
    }

    return it->second.Get();
}

} // namespace hyperion