#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

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

HYP_BEGIN_CLASS(EditorSubsystem, 146, 0, NAME("Subsystem"))
    Method(NAME(HYP_STR(GetCurrentProject)), &EditorSubsystem::GetCurrentProject),
    Method(NAME(HYP_STR(GetActiveScene)), &EditorSubsystem::GetActiveScene),
    Method(NAME(HYP_STR(NewProject)), &EditorSubsystem::NewProject),
    Method(NAME(HYP_STR(OpenProject)), &EditorSubsystem::OpenProject),
    Method(NAME(HYP_STR(ShowOpenProjectDialog)), &EditorSubsystem::ShowOpenProjectDialog),
    Method(NAME(HYP_STR(ShowImportContentDialog)), &EditorSubsystem::ShowImportContentDialog),
    Method(NAME(HYP_STR(AddTask)), &EditorSubsystem::AddTask),
    Method(NAME(HYP_STR(SetFocusedNode)), &EditorSubsystem::SetFocusedNode),
    Method(NAME(HYP_STR(AddDebugOverlay)), &EditorSubsystem::AddDebugOverlay),
    Method(NAME(HYP_STR(RemoveDebugOverlay)), &EditorSubsystem::RemoveDebugOverlay),
    Method(NAME(HYP_STR(GetFocusedNode)), &EditorSubsystem::GetFocusedNode),
    Field(NAME(HYP_STR(OnFocusedNodeChanged)), &EditorSubsystem::OnFocusedNodeChanged, offsetof(EditorSubsystem, OnFocusedNodeChanged), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnProjectClosing)), &EditorSubsystem::OnProjectClosing, offsetof(EditorSubsystem, OnProjectClosing), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnProjectOpened)), &EditorSubsystem::OnProjectOpened, offsetof(EditorSubsystem, OnProjectOpened), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnActiveSceneChanged)), &EditorSubsystem::OnActiveSceneChanged, offsetof(EditorSubsystem, OnActiveSceneChanged), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorSubsystem Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GenerateLightmapsEditorTask Reflection Data

HYP_BEGIN_CLASS(GenerateLightmapsEditorTask, 45, 0, NAME("TickableEditorTask"))
    Method(NAME(HYP_STR(GetWorld)), &GenerateLightmapsEditorTask::GetWorld),
    Method(NAME(HYP_STR(SetWorld)), &GenerateLightmapsEditorTask::SetWorld),
    Method(NAME(HYP_STR(GetScene)), &GenerateLightmapsEditorTask::GetScene),
    Method(NAME(HYP_STR(SetScene)), &GenerateLightmapsEditorTask::SetScene),
    Method(NAME(HYP_STR(GetAABB)), &GenerateLightmapsEditorTask::GetAABB),
    Method(NAME(HYP_STR(SetAABB)), &GenerateLightmapsEditorTask::SetAABB),
    Method(NAME(HYP_STR(Process)), &GenerateLightmapsEditorTask::Process),
    Method(NAME(HYP_STR(Cancel)), &GenerateLightmapsEditorTask::Cancel),
    Method(NAME(HYP_STR(IsCompleted)), &GenerateLightmapsEditorTask::IsCompleted),
    Method(NAME(HYP_STR(Tick)), &GenerateLightmapsEditorTask::Tick)
HYP_END_CLASS

#pragma endregion GenerateLightmapsEditorTask Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorManipulationWidgetBase Reflection Data

HYP_BEGIN_CLASS(EditorManipulationWidgetBase, 39, 2, NAME("ObjectBase"), ClassAttribute("abstract", true))
HYP_END_CLASS

#pragma endregion EditorManipulationWidgetBase Reflection Data

} // namespace hyperion

