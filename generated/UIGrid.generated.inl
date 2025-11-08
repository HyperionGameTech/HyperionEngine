#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UIGridColumn Reflection Data

HYP_BEGIN_CLASS(UIGridColumn, 204, 0, NAME("UIPanel"))
    Method(NAME(HYP_STR(GetColumnSize)), &UIGridColumn::GetColumnSize, Span<const ClassAttribute> { {ClassAttribute("property", "ColumnSize"), ClassAttribute("xmlattribute", "colsize") } }),
    Method(NAME(HYP_STR(SetColumnSize)), &UIGridColumn::SetColumnSize, Span<const ClassAttribute> { {ClassAttribute("property", "ColumnSize"), ClassAttribute("xmlattribute", "colsize") } })
HYP_END_CLASS

#pragma endregion UIGridColumn Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIGridRow Reflection Data

HYP_BEGIN_CLASS(UIGridRow, 205, 0, NAME("UIPanel"))
HYP_END_CLASS

#pragma endregion UIGridRow Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIGrid Reflection Data

HYP_BEGIN_CLASS(UIGrid, 206, 0, NAME("UIPanel"))
    Method(NAME(HYP_STR(GetNumColumns)), &UIGrid::GetNumColumns, Span<const ClassAttribute> { {ClassAttribute("property", "NumColumns") } }),
    Method(NAME(HYP_STR(SetNumColumns)), &UIGrid::SetNumColumns, Span<const ClassAttribute> { {ClassAttribute("property", "NumColumns"), ClassAttribute("xmlattribute", "cols") } }),
    Method(NAME(HYP_STR(GetNumRows)), &UIGrid::GetNumRows, Span<const ClassAttribute> { {ClassAttribute("property", "NumRows") } }),
    Method(NAME(HYP_STR(SetNumRows)), &UIGrid::SetNumRows, Span<const ClassAttribute> { {ClassAttribute("property", "NumRows"), ClassAttribute("xmlattribute", "rows") } })
HYP_END_CLASS

#pragma endregion UIGrid Reflection Data

} // namespace hyperion

