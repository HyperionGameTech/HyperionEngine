#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region WorldGrid Reflection Data

HYP_BEGIN_CLASS(WorldGrid, 176, 0, NAME("HypObjectBase"))
    Method(NAME(HYP_STR(GetWorld)), &WorldGrid::GetWorld),
    Method(NAME(HYP_STR(AddLayer)), &WorldGrid::AddLayer),
    Method(NAME(HYP_STR(RemoveLayer)), &WorldGrid::RemoveLayer),
    Method(NAME(HYP_STR(GetLayers)), &WorldGrid::GetLayers),
    Field(NAME(HYP_STR(Layers)), &WorldGrid::m_layers, offsetof(WorldGrid, m_layers), Span<const ClassAttribute> { {ClassAttribute("property", "Layers"), ClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion WorldGrid Reflection Data

} // namespace hyperion

