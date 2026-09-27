/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/Node.hpp>

#include <Scene/Instancing/InstanceTypes.hpp>

namespace Hyperion {

class InstanceGroup;

/// Stand-in node for editing an instance itself. EditorSubsystem::GetOrCreateInstanceHandle makes and tracks these.
HYP_CLASS(Serialize = false)
class EDITOR_API InstanceHandleNode final : public Node
{
    HYP_OBJECT_BODY(InstanceHandleNode);

public:
    InstanceHandleNode();
    InstanceHandleNode(const WeakHandle<InstanceGroup>& group, InstanceId instanceId);

    ~InstanceHandleNode() override;

    HYP_FORCE_INLINE const WeakHandle<InstanceGroup>& GetGroup() const
    {
        return m_group;
    }

    HYP_FORCE_INLINE InstanceId GetInstanceId() const
    {
        return m_instanceId;
    }

    /// Moves this node to where its instance is without writing back to the group
    void SyncFromInstance();

protected:
    void OnTransformUpdated() override;

private:
    WeakHandle<InstanceGroup> m_group;
    InstanceId m_instanceId;

    bool m_isSyncing;
};

} // namespace Hyperion
