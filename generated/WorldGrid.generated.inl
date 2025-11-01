#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region WorldGrid Reflection Data

HYP_BEGIN_CLASS(WorldGrid, 175, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetWorld)), &WorldGrid::GetWorld),
    HypMethod(NAME(HYP_STR(GetStreamingManager)), &WorldGrid::GetStreamingManager),
    HypMethod(NAME(HYP_STR(AddLayer)), &WorldGrid::AddLayer),
    HypMethod(NAME(HYP_STR(RemoveLayer)), &WorldGrid::RemoveLayer),
    HypMethod(NAME(HYP_STR(GetLayers)), &WorldGrid::GetLayers),
    HypField(NAME(HYP_STR(Layers)), &WorldGrid::m_layers, offsetof(WorldGrid, m_layers), Span<const HypClassAttribute> { {HypClassAttribute("property", "Layers"), HypClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion WorldGrid Reflection Data

} // namespace hyperion

