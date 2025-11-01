#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EditorMain Reflection Data

HYP_BEGIN_CLASS(EditorMain, 46, 0, NAME("HypObjectBase"), HypClassAttribute("noscriptbindings", true))
    HypMethod(NAME(HYP_STR(BeforeAdded)), &EditorMain::BeforeAdded),
    HypMethod(NAME(HYP_STR(OnAdded)), &EditorMain::OnAdded),
    HypMethod(NAME(HYP_STR(OpenProjectClicked)), &EditorMain::OpenProjectClicked),
    HypMethod(NAME(HYP_STR(SaveClicked)), &EditorMain::SaveClicked),
    HypMethod(NAME(HYP_STR(UndoClicked)), &EditorMain::UndoClicked),
    HypMethod(NAME(HYP_STR(RedoClicked)), &EditorMain::RedoClicked),
    HypMethod(NAME(HYP_STR(UpdateUndoMenuItem)), &EditorMain::UpdateUndoMenuItem),
    HypMethod(NAME(HYP_STR(UpdateRedoMenuItem)), &EditorMain::UpdateRedoMenuItem),
    HypMethod(NAME(HYP_STR(SimulateClicked)), &EditorMain::SimulateClicked),
    HypMethod(NAME(HYP_STR(RebuildLightmaps)), &EditorMain::RebuildLightmaps),
    HypMethod(NAME(HYP_STR(AddPointLight)), &EditorMain::AddPointLight),
    HypMethod(NAME(HYP_STR(AddAreaRectLight)), &EditorMain::AddAreaRectLight),
    HypMethod(NAME(HYP_STR(AddReflectionProbe)), &EditorMain::AddReflectionProbe),
    HypMethod(NAME(HYP_STR(AddLightmapVolume)), &EditorMain::AddLightmapVolume),
    HypMethod(NAME(HYP_STR(AddNode)), &EditorMain::AddNode),
    HypMethod(NAME(HYP_STR(AddEntity)), &EditorMain::AddEntity)
HYP_END_CLASS

#pragma endregion EditorMain Reflection Data

} // namespace hyperion

