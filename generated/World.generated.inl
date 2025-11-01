#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region World Reflection Data

HYP_BEGIN_CLASS(World, 164, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetName)), &World::GetName),
    HypMethod(NAME(HYP_STR(SetName)), &World::SetName),
    HypMethod(NAME(HYP_STR(GetRenderStats)), &World::GetRenderStats),
    HypMethod(NAME(HYP_STR(GetSubsystemByName)), &World::GetSubsystemByName),
    HypMethod(NAME(HYP_STR(RemoveSubsystem)), &World::RemoveSubsystem),
    HypMethod(NAME(HYP_STR(GetWorldGrid)), &World::GetWorldGrid),
    HypMethod(NAME(HYP_STR(GetGameState)), &World::GetGameState),
    HypMethod(NAME(HYP_STR(StartSimulating)), &World::StartSimulating),
    HypMethod(NAME(HYP_STR(StopSimulating)), &World::StopSimulating),
    HypMethod(NAME(HYP_STR(AddScene)), &World::AddScene),
    HypMethod(NAME(HYP_STR(RemoveScene)), &World::RemoveScene),
    HypMethod(NAME(HYP_STR(HasScene)), &World::HasScene),
    HypMethod(NAME(HYP_STR(GetSceneByName)), &World::GetSceneByName),
    HypMethod(NAME(HYP_STR(AddView)), &World::AddView),
    HypMethod(NAME(HYP_STR(RemoveView)), &World::RemoveView),
    HypField(NAME(HYP_STR(Name)), &World::m_name, offsetof(World, m_name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name"), HypClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion World Reflection Data

} // namespace hyperion

