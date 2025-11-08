#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region StreamingManager Reflection Data

HYP_BEGIN_CLASS(StreamingManager, 68, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(AddStreamingVolume)), &StreamingManager::AddStreamingVolume),
    Method(NAME(HYP_STR(RemoveStreamingVolume)), &StreamingManager::RemoveStreamingVolume)
HYP_END_CLASS

#pragma endregion StreamingManager Reflection Data

} // namespace hyperion

