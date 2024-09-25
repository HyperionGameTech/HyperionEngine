/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RUNTIME_DOTNET_MANAGED_NODE_HPP
#define HYPERION_RUNTIME_DOTNET_MANAGED_NODE_HPP

#include <core/utilities/TypeID.hpp>
#include <core/Handle.hpp>
#include <core/ID.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>

#include <type_traits>

namespace hyperion {

extern "C" struct ManagedNode
{
    RC<Node>::RefCountDataType  *ref;

    /*! \brief Decrements the reference count of the managed node. To be called from C# on garbage collection. */
    void Dispose();

    Node *GetNode();
    const Node *GetNode() const;
};

static_assert(std::is_trivial_v<ManagedNode>, "ManagedNode must be a trivial type to be used in C#");

} // namespace hyperion

#endif