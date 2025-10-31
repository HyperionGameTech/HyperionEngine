#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIGridColumn Reflection Data

HYP_BEGIN_CLASS(UIGridColumn, 8, 0, NAME("UIPanel"))
    HypMethod(NAME(HYP_STR(GetColumnSize)), &UIGridColumn::GetColumnSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "ColumnSize"), HypClassAttribute("xmlattribute", "colsize") } }),
    HypMethod(NAME(HYP_STR(SetColumnSize)), &UIGridColumn::SetColumnSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "ColumnSize"), HypClassAttribute("xmlattribute", "colsize") } })
HYP_END_CLASS

#pragma endregion UIGridColumn Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIGridRow Reflection Data

HYP_BEGIN_CLASS(UIGridRow, 9, 0, NAME("UIPanel"))
HYP_END_CLASS

#pragma endregion UIGridRow Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIGrid Reflection Data

HYP_BEGIN_CLASS(UIGrid, 10, 0, NAME("UIPanel"))
    HypMethod(NAME(HYP_STR(GetNumColumns)), &UIGrid::GetNumColumns, Span<const HypClassAttribute> { {HypClassAttribute("property", "NumColumns") } }),
    HypMethod(NAME(HYP_STR(SetNumColumns)), &UIGrid::SetNumColumns, Span<const HypClassAttribute> { {HypClassAttribute("property", "NumColumns"), HypClassAttribute("xmlattribute", "cols") } }),
    HypMethod(NAME(HYP_STR(GetNumRows)), &UIGrid::GetNumRows, Span<const HypClassAttribute> { {HypClassAttribute("property", "NumRows") } }),
    HypMethod(NAME(HYP_STR(SetNumRows)), &UIGrid::SetNumRows, Span<const HypClassAttribute> { {HypClassAttribute("property", "NumRows"), HypClassAttribute("xmlattribute", "rows") } })
HYP_END_CLASS

#pragma endregion UIGrid Reflection Data

} // namespace hyperion

