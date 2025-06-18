/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/ObjectPool.hpp>

namespace hyperion {

ObjectPool::ObjectContainerMap& ObjectPool::GetObjectContainerHolder()
{
    static ObjectPool::ObjectContainerMap holder {};

    return holder;
}

ObjectContainerBase& ObjectPool::ObjectContainerMap::Get(TypeID type_id)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([type_id](const auto& element)
        {
            return element.first == type_id;
        });

    if (it == m_map.End())
    {
        HYP_FAIL("No object container for TypeID: %u", type_id.Value());
    }

    AssertThrow(it->second != nullptr);

    return *it->second;
}

ObjectContainerBase* ObjectPool::ObjectContainerMap::TryGet(TypeID type_id)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([type_id](const auto& element)
        {
            return element.first == type_id;
        });

    if (it == m_map.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

} // namespace hyperion