#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region TerrainStreamingCell Reflection Data

HYP_BEGIN_CLASS(TerrainStreamingCell, 180, 0, NAME("StreamingCell"), HypClassAttribute("noscriptbindings", true))
    HypMethod(NAME(HYP_STR(OnStreamStart_Impl)), &TerrainStreamingCell::OnStreamStart_Impl),
    HypMethod(NAME(HYP_STR(OnLoaded_Impl)), &TerrainStreamingCell::OnLoaded_Impl),
    HypMethod(NAME(HYP_STR(OnRemoved_Impl)), &TerrainStreamingCell::OnRemoved_Impl)
HYP_END_CLASS

#pragma endregion TerrainStreamingCell Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TerrainWorldGridLayer Reflection Data

HYP_BEGIN_CLASS(TerrainWorldGridLayer, 177, 0, NAME("WorldGridLayer"), HypClassAttribute("noscriptbindings", true))
    HypMethod(NAME(HYP_STR(GetScene)), &TerrainWorldGridLayer::GetScene),
    HypMethod(NAME(HYP_STR(Init)), &TerrainWorldGridLayer::Init),
    HypMethod(NAME(HYP_STR(OnAdded_Impl)), &TerrainWorldGridLayer::OnAdded_Impl),
    HypMethod(NAME(HYP_STR(OnRemoved_Impl)), &TerrainWorldGridLayer::OnRemoved_Impl),
    HypMethod(NAME(HYP_STR(CreateStreamingCell_Impl)), &TerrainWorldGridLayer::CreateStreamingCell_Impl)
HYP_END_CLASS

#pragma endregion TerrainWorldGridLayer Reflection Data

} // namespace hyperion

