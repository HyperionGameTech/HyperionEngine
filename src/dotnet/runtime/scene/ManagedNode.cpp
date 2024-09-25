/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/scene/ManagedNode.hpp>

namespace hyperion {

void ManagedNode::Dispose()
{
    if (!ref) {
        return;
    }

    RC<Node> rc;

    // Take ownership of the ref count data, so don't increment the ref count immediately
    rc.SetRefCountData_Internal(ref, false /* inc_ref */);

    ref = nullptr;

    // Decrement the ref count, data will be deleted if the ref count reaches 0
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

} // namespace hyperion

extern "C" {
HYP_EXPORT void ManagedNode_Dispose(hyperion::ManagedNode managed_node)
{
    managed_node.Dispose();
}
} // extern "C"