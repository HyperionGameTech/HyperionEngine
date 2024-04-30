/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/ObjectPool.hpp>

namespace hyperion {

ObjectPool::ObjectContainerMap ObjectPool::ObjectContainerHolder::s_object_container_map = { };
ObjectPool::ObjectContainerHolder ObjectPool::s_object_container_holder = { };

ObjectPool::ObjectContainerHolder &ObjectPool::GetObjectContainerHolder()
{
    return s_object_container_holder;
}

UniquePtr<ObjectContainerBase> *ObjectPool::ObjectContainerHolder::AllotObjectContainer(TypeID type_id)
{
    // Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    auto it = s_object_container_map.map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    // Already allocated
    if (it != s_object_container_map.map.End()) {
        return &it->second;
    }

    s_object_container_map.map.PushBack({
        type_id,
        nullptr
    });

    return &s_object_container_map.map.Back().second;
}

ObjectContainerBase *ObjectPool::ObjectContainerHolder::TryGetObjectContainer(TypeID type_id)
{
    Mutex::Guard guard(s_object_container_map.mutex);

    const auto it = s_object_container_map.map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    if (it == s_object_container_map.map.End()) {
        return nullptr;
    }

    return it->second.Get();
}

} // namespace hyperion