#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Scene Reflection Data

HYP_BEGIN_CLASS(Scene, 53, 0, NAME("AssetObject"), HypClassAttribute("postload", "Scene_OnPostLoad"))
    HypMethod(NAME(HYP_STR(GetPrimaryCamera)), &Scene::GetPrimaryCamera),
    HypMethod(NAME(HYP_STR(GetSceneFlags)), &Scene::GetSceneFlags),
    HypMethod(NAME(HYP_STR(SetSceneFlags)), &Scene::SetSceneFlags),
    HypMethod(NAME(HYP_STR(FindNodeByName)), &Scene::FindNodeByName),
    HypMethod(NAME(HYP_STR(GetRoot)), &Scene::GetRoot, Span<const HypClassAttribute> { {HypClassAttribute("property", "Root"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetRoot)), &Scene::SetRoot, Span<const HypClassAttribute> { {HypClassAttribute("property", "Root"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetEntityManager)), &Scene::GetEntityManager),
    HypMethod(NAME(HYP_STR(IsAttachedToWorld)), &Scene::IsAttachedToWorld),
    HypMethod(NAME(HYP_STR(GetWorld)), &Scene::GetWorld),
    HypMethod(NAME(HYP_STR(SetWorld)), &Scene::SetWorld),
    HypMethod(NAME(HYP_STR(IsForegroundScene)), &Scene::IsForegroundScene),
    HypMethod(NAME(HYP_STR(IsBackgroundScene)), &Scene::IsBackgroundScene),
    HypMethod(NAME(HYP_STR(IsAudioListener)), &Scene::IsAudioListener),
    HypMethod(NAME(HYP_STR(SetIsAudioListener)), &Scene::SetIsAudioListener),
    HypMethod(NAME(HYP_STR(AddToWorld)), &Scene::AddToWorld),
    HypMethod(NAME(HYP_STR(RemoveFromWorld)), &Scene::RemoveFromWorld),
    HypMethod(NAME(HYP_STR(GetUniqueNodeName)), &Scene::GetUniqueNodeName),
    HypField(NAME(HYP_STR(SceneFlags)), &Scene::m_sceneFlags, offsetof(Scene, m_sceneFlags), Span<const HypClassAttribute> { {HypClassAttribute("property", "SceneFlags") } }),
    HypField(NAME(HYP_STR(Root)), &Scene::m_root, offsetof(Scene, m_root), Span<const HypClassAttribute> { {HypClassAttribute("property", "Root") } }),
    HypField(NAME(HYP_STR(OwnerThreadId)), &Scene::m_ownerThreadId, offsetof(Scene, m_ownerThreadId), Span<const HypClassAttribute> { {HypClassAttribute("property", "OwnerThreadId"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(World)), &Scene::m_world, offsetof(Scene, m_world), Span<const HypClassAttribute> { {HypClassAttribute("property", "World"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(EntityManager)), &Scene::m_entityManager, offsetof(Scene, m_entityManager), Span<const HypClassAttribute> { {HypClassAttribute("property", "EntityManager"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(IsAudioListener)), &Scene::m_isAudioListener, offsetof(Scene, m_isAudioListener), Span<const HypClassAttribute> { {HypClassAttribute("property", "IsAudioListener") } }),
    HypField(NAME(HYP_STR(PreviousDelta)), &Scene::m_previousDelta, offsetof(Scene, m_previousDelta), Span<const HypClassAttribute> { {HypClassAttribute("property", "PreviousDelta"), HypClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion Scene Reflection Data

static const HypClassCallbackRegistration<HypClassCallbackType::ON_POST_LOAD> g_post_load_Scene(TypeId::ForType<Scene>(), ValueWrapper<Scene_OnPostLoad>());
} // namespace hyperion


namespace hyperion {

#pragma region SceneFlags Reflection Data

HYP_BEGIN_ENUM(SceneFlags, 383, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), SceneFlags::NONE),
    HypConstant(NAME(HYP_STR(FOREGROUND)), SceneFlags::FOREGROUND),
    HypConstant(NAME(HYP_STR(DETACHED)), SceneFlags::DETACHED),
    HypConstant(NAME(HYP_STR(UI)), SceneFlags::UI),
    HypConstant(NAME(HYP_STR(EDITOR)), SceneFlags::EDITOR)
HYP_END_ENUM

#pragma endregion SceneFlags Reflection Data

} // namespace hyperion

