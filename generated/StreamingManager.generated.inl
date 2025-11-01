#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region StreamingManager Reflection Data

HYP_BEGIN_CLASS(StreamingManager, 181, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(AddStreamingVolume)), &StreamingManager::AddStreamingVolume),
    HypMethod(NAME(HYP_STR(RemoveStreamingVolume)), &StreamingManager::RemoveStreamingVolume)
HYP_END_CLASS

#pragma endregion StreamingManager Reflection Data

} // namespace hyperion

