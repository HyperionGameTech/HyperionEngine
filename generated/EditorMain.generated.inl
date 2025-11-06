#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EditorMain Reflection Data

HYP_BEGIN_CLASS(EditorMain, 46, 0, NAME("HypObjectBase"), ClassAttribute("noscriptbindings", true))
    Method(NAME(HYP_STR(BeforeAdded)), &EditorMain::BeforeAdded),
    Method(NAME(HYP_STR(OnAdded)), &EditorMain::OnAdded),
    Method(NAME(HYP_STR(OpenProjectClicked)), &EditorMain::OpenProjectClicked),
    Method(NAME(HYP_STR(SaveClicked)), &EditorMain::SaveClicked),
    Method(NAME(HYP_STR(UndoClicked)), &EditorMain::UndoClicked),
    Method(NAME(HYP_STR(RedoClicked)), &EditorMain::RedoClicked),
    Method(NAME(HYP_STR(UpdateUndoMenuItem)), &EditorMain::UpdateUndoMenuItem),
    Method(NAME(HYP_STR(UpdateRedoMenuItem)), &EditorMain::UpdateRedoMenuItem),
    Method(NAME(HYP_STR(SimulateClicked)), &EditorMain::SimulateClicked),
    Method(NAME(HYP_STR(RebuildLightmaps)), &EditorMain::RebuildLightmaps),
    Method(NAME(HYP_STR(AddPointLight)), &EditorMain::AddPointLight),
    Method(NAME(HYP_STR(AddAreaRectLight)), &EditorMain::AddAreaRectLight),
    Method(NAME(HYP_STR(AddReflectionProbe)), &EditorMain::AddReflectionProbe),
    Method(NAME(HYP_STR(AddLightmapVolume)), &EditorMain::AddLightmapVolume),
    Method(NAME(HYP_STR(AddNode)), &EditorMain::AddNode),
    Method(NAME(HYP_STR(AddEntity)), &EditorMain::AddEntity)
HYP_END_CLASS

#pragma endregion EditorMain Reflection Data

} // namespace hyperion

