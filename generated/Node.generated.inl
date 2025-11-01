#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region NodeFlags Reflection Data

HYP_BEGIN_ENUM(NodeFlags, 361, 0, {}, HypClassAttribute("flags", true))
    HypConstant(NAME(HYP_STR(NONE)), NodeFlags::NONE),
    HypConstant(NAME(HYP_STR(IGNORE_PARENT_TRANSLATION)), NodeFlags::IGNORE_PARENT_TRANSLATION),
    HypConstant(NAME(HYP_STR(IGNORE_PARENT_SCALE)), NodeFlags::IGNORE_PARENT_SCALE),
    HypConstant(NAME(HYP_STR(IGNORE_PARENT_ROTATION)), NodeFlags::IGNORE_PARENT_ROTATION),
    HypConstant(NAME(HYP_STR(IGNORE_PARENT_TRANSFORM)), NodeFlags::IGNORE_PARENT_TRANSFORM),
    HypConstant(NAME(HYP_STR(EXCLUDE_FROM_PARENT_AABB)), NodeFlags::EXCLUDE_FROM_PARENT_AABB),
    HypConstant(NAME(HYP_STR(TRANSIENT)), NodeFlags::TRANSIENT),
    HypConstant(NAME(HYP_STR(HIDE_IN_SCENE_OUTLINE)), NodeFlags::HIDE_IN_SCENE_OUTLINE)
HYP_END_ENUM

#pragma endregion NodeFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region NodeTagSet Reflection Data

HYP_BEGIN_STRUCT(NodeTagSet, 362, 0, {})
HYP_END_STRUCT

#pragma endregion NodeTagSet Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region NodeTag Reflection Data

HYP_BEGIN_STRUCT(NodeTag, 363, 0, {})
    HypField(NAME(HYP_STR(Name)), &NodeTag::name, offsetof(NodeTag, name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Data)), &NodeTag::data, offsetof(NodeTag, data), Span<const HypClassAttribute> { {HypClassAttribute("property", "Data"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion NodeTag Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Node Reflection Data

HYP_BEGIN_CLASS(Node, 131, 14, NAME("HypObjectBase"), HypClassAttribute("postload", "Node_OnPostLoad"))
    HypMethod(NAME(HYP_STR(GetUUID)), &Node::GetUUID),
    HypMethod(NAME(HYP_STR(GetName)), &Node::GetName),
    HypMethod(NAME(HYP_STR(SetName)), &Node::SetName),
    HypMethod(NAME(HYP_STR(HasName)), &Node::HasName),
    HypMethod(NAME(HYP_STR(GetNodeFlags)), &Node::GetNodeFlags, Span<const HypClassAttribute> { {HypClassAttribute("property", "NodeFlags"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetNodeFlags)), &Node::SetNodeFlags, Span<const HypClassAttribute> { {HypClassAttribute("property", "NodeFlags") } }),
    HypMethod(NAME(HYP_STR(GetParent)), &Node::GetParent, Span<const HypClassAttribute> { {HypClassAttribute("property", "Parent"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(IsOrHasParent)), &Node::IsOrHasParent),
    HypMethod(NAME(HYP_STR(FindParentWithName)), &Node::FindParentWithName),
    HypMethod(NAME(HYP_STR(IsRoot)), &Node::IsRoot),
    HypMethod(NAME(HYP_STR(GetScene)), &Node::GetScene, Span<const HypClassAttribute> { {HypClassAttribute("property", "Scene"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetScene)), &Node::SetScene, Span<const HypClassAttribute> { {HypClassAttribute("property", "Scene"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetWorld)), &Node::GetWorld, Span<const HypClassAttribute> { {HypClassAttribute("property", "World"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetEntityAABB)), &Node::GetEntityAABB, Span<const HypClassAttribute> { {HypClassAttribute("property", "Aabb"), HypClassAttribute("editor", true), HypClassAttribute("label", "Bounding Box"), HypClassAttribute("description", "The underlying axis-aligned bounding box for this node, not considering child nodes or transform") } }),
    HypMethod(NAME(HYP_STR(SetEntityAABB)), &Node::SetEntityAABB, Span<const HypClassAttribute> { {HypClassAttribute("property", "Aabb"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(AddChild)), &Node::AddChild),
    HypMethod(NAME(HYP_STR(RemoveAt)), &Node::RemoveAt),
    HypMethod(NAME(HYP_STR(Remove)), &Node::Remove),
    HypMethod(NAME(HYP_STR(RemoveAllChildren)), &Node::RemoveAllChildren),
    HypMethod(NAME(HYP_STR(GetChild)), &Node::GetChild),
    HypMethod(NAME(HYP_STR(Select)), &Node::Select),
    HypMethod(NAME(HYP_STR(SetLocalTransform)), &Node::SetLocalTransform, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalTransform"), HypClassAttribute("editor", true), HypClassAttribute("label", "Local-space Transform"), HypClassAttribute("editorpropertypanelclass", "TransformEditorPropertyPanel") } }),
    HypMethod(NAME(HYP_STR(GetLocalTransform)), &Node::GetLocalTransform, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalTransform"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetLocalTranslation)), &Node::GetLocalTranslation, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalTranslation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetLocalTranslation)), &Node::SetLocalTranslation, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalTranslation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(Translate)), &Node::Translate),
    HypMethod(NAME(HYP_STR(GetLocalScale)), &Node::GetLocalScale, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalScale"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetLocalScale)), &Node::SetLocalScale, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalScale"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(Scale)), &Node::Scale),
    HypMethod(NAME(HYP_STR(GetLocalRotation)), &Node::GetLocalRotation, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalRotation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetLocalRotation)), &Node::SetLocalRotation, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalRotation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(Rotate)), &Node::Rotate),
    HypMethod(NAME(HYP_STR(GetWorldTransform)), &Node::GetWorldTransform, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldTransform"), HypClassAttribute("transient", true), HypClassAttribute("editor", true), HypClassAttribute("label", "World-space Transform"), HypClassAttribute("editorpropertypanelclass", "TransformEditorPropertyPanel") } }),
    HypMethod(NAME(HYP_STR(SetWorldTransform)), &Node::SetWorldTransform, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldTransform"), HypClassAttribute("transient", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetWorldTranslation)), &Node::GetWorldTranslation, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldTranslation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetWorldTranslation)), &Node::SetWorldTranslation, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldTranslation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetWorldScale)), &Node::GetWorldScale, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldScale"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetWorldScale)), &Node::SetWorldScale, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldScale"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetWorldRotation)), &Node::GetWorldRotation, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldRotation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetWorldRotation)), &Node::SetWorldRotation, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldRotation"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetRelativeTransform)), &Node::GetRelativeTransform),
    HypMethod(NAME(HYP_STR(IsTransformLocked)), &Node::IsTransformLocked),
    HypMethod(NAME(HYP_STR(GetLocalAABBExcludingSelf)), &Node::GetLocalAABBExcludingSelf),
    HypMethod(NAME(HYP_STR(GetLocalAABB)), &Node::GetLocalAABB, Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalAabb"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetWorldAABB)), &Node::GetWorldAABB, Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldAabb"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(UpdateWorldTransform)), &Node::UpdateWorldTransform),
    HypMethod(NAME(HYP_STR(CalculateDepth)), &Node::CalculateDepth),
    HypMethod(NAME(HYP_STR(FindSelfIndex)), &Node::FindSelfIndex),
    HypMethod(NAME(HYP_STR(FindChildByName)), &Node::FindChildByName),
    HypMethod(NAME(HYP_STR(FindChildByUUID)), &Node::FindChildByUUID),
    HypField(NAME(HYP_STR(Name)), &Node::m_name, offsetof(Node, m_name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name"), HypClassAttribute("editor", true), HypClassAttribute("label", "Name"), HypClassAttribute("description", "The name of the node.") } }),
    HypField(NAME(HYP_STR(ParentNode)), &Node::m_parentNode, offsetof(Node, m_parentNode), Span<const HypClassAttribute> { {HypClassAttribute("property", "Parent"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(ChildNodes)), &Node::m_childNodes, offsetof(Node, m_childNodes), Span<const HypClassAttribute> { {HypClassAttribute("property", "Children"), HypClassAttribute("loadorder", -1) } }),
    HypField(NAME(HYP_STR(LocalTransform)), &Node::m_localTransform, offsetof(Node, m_localTransform), Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalTransform") } }),
    HypField(NAME(HYP_STR(WorldTransform)), &Node::m_worldTransform, offsetof(Node, m_worldTransform), Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalTransform") } }),
    HypField(NAME(HYP_STR(EntityAabb)), &Node::m_entityAabb, offsetof(Node, m_entityAabb), Span<const HypClassAttribute> { {HypClassAttribute("property", "Aabb") } }),
    HypField(NAME(HYP_STR(Scene)), &Node::m_scene, offsetof(Node, m_scene), Span<const HypClassAttribute> { {HypClassAttribute("property", "Scene"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Tags)), &Node::m_tags, offsetof(Node, m_tags), Span<const HypClassAttribute> { {HypClassAttribute("property", "NodeTags") } }),
    HypField(NAME(HYP_STR(Uuid)), &Node::m_uuid, offsetof(Node, m_uuid), Span<const HypClassAttribute> { {HypClassAttribute("property", "Uuid") } })
HYP_END_CLASS

#pragma endregion Node Reflection Data

static const HypClassCallbackRegistration<HypClassCallbackType::ON_POST_LOAD> g_post_load_Node(TypeId::ForType<Node>(), ValueWrapper<Node_OnPostLoad>());
} // namespace hyperion

