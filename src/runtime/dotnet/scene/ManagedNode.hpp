#ifndef HYPERION_V2_RUNTIME_DOTNET_MANAGED_NODE_HPP
#define HYPERION_V2_RUNTIME_DOTNET_MANAGED_NODE_HPP

#include <core/lib/TypeID.hpp>
#include <core/Handle.hpp>
#include <core/ID.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>

#include <type_traits>

namespace hyperion::v2 {

extern "C" struct ManagedNode
{
    RC<Node>::RefCountDataType  *ref;

    /*! \brief Decrements the reference count of the managed node. To be called from C# on garbage collection. */
    void Dispose();

    Node *GetNode();
    const Node *GetNode() const;
};

static_assert(std::is_trivial_v<ManagedNode>, "ManagedNode must be a trivial type to be used in C#");

static inline ManagedNode CreateManagedNodeFromNodeProxy(NodeProxy node_proxy)
{
    // hacky cast to get around private inheritance
    RC<Node> *rc = reinterpret_cast<RC<Node> *>(&node_proxy);

    // Take ownership of the ref count data
    RC<Node>::RefCountDataType *ref_count_data = rc->Release();

    return { ref_count_data };
}

static inline NodeProxy CreateNodeProxyFromManagedNode(ManagedNode managed_node)
{
    RC<Node> rc;
    rc.SetRefCountData(managed_node.ref);

    return NodeProxy(std::move(rc));
}

} // namespace hyperion::v2

#endif