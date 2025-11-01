#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region NullEditorManipulationWidget Reflection Data

HYP_BEGIN_CLASS(NullEditorManipulationWidget, 40, 0, NAME("EditorManipulationWidgetBase"))
HYP_END_CLASS

#pragma endregion NullEditorManipulationWidget Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TranslateEditorManipulationWidget Reflection Data

HYP_BEGIN_CLASS(TranslateEditorManipulationWidget, 41, 0, NAME("EditorManipulationWidgetBase"))
HYP_END_CLASS

#pragma endregion TranslateEditorManipulationWidget Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorSubsystem Reflection Data

HYP_BEGIN_CLASS(EditorSubsystem, 147, 0, NAME("Subsystem"))
    HypMethod(NAME(HYP_STR(GetCurrentProject)), &EditorSubsystem::GetCurrentProject),
    HypMethod(NAME(HYP_STR(GetActiveScene)), &EditorSubsystem::GetActiveScene),
    HypMethod(NAME(HYP_STR(NewProject)), &EditorSubsystem::NewProject),
    HypMethod(NAME(HYP_STR(OpenProject)), &EditorSubsystem::OpenProject),
    HypMethod(NAME(HYP_STR(ShowOpenProjectDialog)), &EditorSubsystem::ShowOpenProjectDialog),
    HypMethod(NAME(HYP_STR(ShowImportContentDialog)), &EditorSubsystem::ShowImportContentDialog),
    HypMethod(NAME(HYP_STR(AddTask)), &EditorSubsystem::AddTask),
    HypMethod(NAME(HYP_STR(SetFocusedNode)), &EditorSubsystem::SetFocusedNode),
    HypMethod(NAME(HYP_STR(AddDebugOverlay)), &EditorSubsystem::AddDebugOverlay),
    HypMethod(NAME(HYP_STR(RemoveDebugOverlay)), &EditorSubsystem::RemoveDebugOverlay),
    HypMethod(NAME(HYP_STR(GetFocusedNode)), &EditorSubsystem::GetFocusedNode),
    HypField(NAME(HYP_STR(OnFocusedNodeChanged)), &EditorSubsystem::OnFocusedNodeChanged, offsetof(EditorSubsystem, OnFocusedNodeChanged), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnProjectClosing)), &EditorSubsystem::OnProjectClosing, offsetof(EditorSubsystem, OnProjectClosing), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnProjectOpened)), &EditorSubsystem::OnProjectOpened, offsetof(EditorSubsystem, OnProjectOpened), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnActiveSceneChanged)), &EditorSubsystem::OnActiveSceneChanged, offsetof(EditorSubsystem, OnActiveSceneChanged), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorSubsystem Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GenerateLightmapsEditorTask Reflection Data

HYP_BEGIN_CLASS(GenerateLightmapsEditorTask, 45, 0, NAME("TickableEditorTask"))
    HypMethod(NAME(HYP_STR(GetWorld)), &GenerateLightmapsEditorTask::GetWorld),
    HypMethod(NAME(HYP_STR(SetWorld)), &GenerateLightmapsEditorTask::SetWorld),
    HypMethod(NAME(HYP_STR(GetScene)), &GenerateLightmapsEditorTask::GetScene),
    HypMethod(NAME(HYP_STR(SetScene)), &GenerateLightmapsEditorTask::SetScene),
    HypMethod(NAME(HYP_STR(GetAABB)), &GenerateLightmapsEditorTask::GetAABB),
    HypMethod(NAME(HYP_STR(SetAABB)), &GenerateLightmapsEditorTask::SetAABB),
    HypMethod(NAME(HYP_STR(Process)), &GenerateLightmapsEditorTask::Process),
    HypMethod(NAME(HYP_STR(Cancel)), &GenerateLightmapsEditorTask::Cancel),
    HypMethod(NAME(HYP_STR(IsCompleted)), &GenerateLightmapsEditorTask::IsCompleted),
    HypMethod(NAME(HYP_STR(Tick)), &GenerateLightmapsEditorTask::Tick)
HYP_END_CLASS

#pragma endregion GenerateLightmapsEditorTask Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorManipulationWidgetBase Reflection Data

HYP_BEGIN_CLASS(EditorManipulationWidgetBase, 39, 2, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
HYP_END_CLASS

#pragma endregion EditorManipulationWidgetBase Reflection Data

} // namespace hyperion

