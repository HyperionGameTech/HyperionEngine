#include <core/ObjectPool.hpp>

namespace hyperion::v2 {

ObjectPool::ObjectContainerHolder::ObjectContainerMap ObjectPool::ObjectContainerHolder::s_object_container_map = { };
ObjectPool::ObjectContainerHolder ObjectPool::s_object_container_holder = { };

} // namespace hyperion::v2