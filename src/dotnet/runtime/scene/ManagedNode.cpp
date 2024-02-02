#include <dotnet/runtime/scene/ManagedNode.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void ManagedNode::Dispose()
{
    if (!ref) {
        return;
    }

    RC<Node> rc;

    // Take ownership of the ref count data, so don't increment the ref count immediately
    rc.SetRefCountData(ref, false /* inc_ref */);

    ref = nullptr;

    rc.Reset();
}

Node *ManagedNode::GetNode()
{
    if (!ref) {
        return nullptr;
    }

    return static_cast<Node *>(ref->value);
}

const Node *ManagedNode::GetNode() const
{
    if (!ref) {
        return nullptr;
    }

    return static_cast<const Node *>(ref->value);
}

} // namespace hyperion::v2

extern "C" {
    void ManagedNode_Dispose(hyperion::v2::ManagedNode managed_node)
    {
        managed_node.Dispose();
    }
}