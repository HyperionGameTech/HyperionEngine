#include <core/ObjectPool.hpp>

namespace hyperion::v2 {

HYP_API ObjectPool::ObjectContainerHolder::ObjectContainerMap ObjectPool::ObjectContainerHolder::s_object_container_map = { };
HYP_API ObjectPool::ObjectContainerHolder ObjectPool::s_object_container_holder = { };

} // namespace hyperion::v2