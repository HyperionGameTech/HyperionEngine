/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Instancing/InstanceHandleNode.hpp>

#include <Scene/Instancing/InstanceGroup.hpp>

#include <InstanceHandleNode.generated.inl>

namespace Hyperion {

InstanceHandleNode::InstanceHandleNode()
    : m_instanceId(InvalidInstanceId),
      m_isSyncing(false)
{
    m_nodeFlags |= NodeFlags::HideInSceneOutline;
}

InstanceHandleNode::InstanceHandleNode(const WeakHandle<InstanceGroup>& group, InstanceId instanceId)
    : InstanceHandleNode()
{
    m_group = group;
    m_instanceId = instanceId;
}

InstanceHandleNode::~InstanceHandleNode() = default;

void InstanceHandleNode::SyncFromInstance()
{
    Handle<InstanceGroup> group = m_group.Lock();

    if (!group.IsValid())
    {
        return;
    }

    Transform transform;

    if (!group->GetInstanceTransform(m_instanceId, transform))
    {
        return;
    }

    SetLocalBounds(group->GetPrefabBounds());

    m_isSyncing = true;
    SetLocalTransform(transform);
    m_isSyncing = false;
}

void InstanceHandleNode::OnTransformUpdated()
{
    Node::OnTransformUpdated();

    if (m_isSyncing)
    {
        return;
    }

    Handle<InstanceGroup> group = m_group.Lock();

    if (!group.IsValid())
    {
        return;
    }

    // a no-op once the instance is gone, e.g. an undo of a move after the instance was deleted
    group->SetInstanceTransform(m_instanceId, Transform(GetWorldTranslation(), GetWorldScale(), GetWorldRotation()));
}

} // namespace Hyperion
