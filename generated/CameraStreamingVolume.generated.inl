#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region CameraStreamingVolume Reflection Data

HYP_BEGIN_CLASS(CameraStreamingVolume, 183, 0, NAME("StreamingVolumeBase"), ClassAttribute("noscriptbindings", true))
    Method(NAME(HYP_STR(GetShape_Impl)), &CameraStreamingVolume::GetShape_Impl),
    Method(NAME(HYP_STR(GetBoundingBox_Impl)), &CameraStreamingVolume::GetBoundingBox_Impl),
    Method(NAME(HYP_STR(GetBoundingSphere_Impl)), &CameraStreamingVolume::GetBoundingSphere_Impl),
    Method(NAME(HYP_STR(ContainsPoint_Impl)), &CameraStreamingVolume::ContainsPoint_Impl)
HYP_END_CLASS

#pragma endregion CameraStreamingVolume Reflection Data

} // namespace hyperion

