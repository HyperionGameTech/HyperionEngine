/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

} // namespace hyperion::v2

extern "C" {
HYP_EXPORT void ManagedNode_Dispose(hyperion::v2::ManagedNode managed_node)
{
    managed_node.Dispose();
}
} // extern "C"