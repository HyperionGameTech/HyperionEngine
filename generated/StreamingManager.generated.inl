#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region StreamingManager Reflection Data

HYP_BEGIN_CLASS(StreamingManager, 182, 0, NAME("HypObjectBase"))
    Method(NAME(HYP_STR(AddStreamingVolume)), &StreamingManager::AddStreamingVolume),
    Method(NAME(HYP_STR(RemoveStreamingVolume)), &StreamingManager::RemoveStreamingVolume)
HYP_END_CLASS

#pragma endregion StreamingManager Reflection Data

} // namespace hyperion

