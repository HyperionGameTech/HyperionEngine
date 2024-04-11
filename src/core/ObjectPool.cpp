#include <core/ObjectPool.hpp>

namespace hyperion::v2 {

ObjectPool::ObjectContainerMap ObjectPool::ObjectContainerHolder::s_object_container_map = { };
ObjectPool::ObjectContainerHolder ObjectPool::s_object_container_holder = { };

auto ObjectPool::ObjectContainerHolder::GetObjectContainerMap() -> ObjectContainerMap &
{
    return s_object_container_map;
}

ObjectContainerBase *ObjectPool::ObjectContainerHolder::TryGetObjectContainer(TypeID type_id)
{
    Mutex::Guard guard(s_object_container_map.mutex);

    const auto it = s_object_container_map.map.Find(type_id);

    if (it == s_object_container_map.map.End()) {
        return nullptr;
    }

    return it->second;
}

} // namespace hyperion::v2