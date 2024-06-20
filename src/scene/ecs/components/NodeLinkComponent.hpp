/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_NODE_LINK_COMPONENT_HPP
#define HYPERION_ECS_NODE_LINK_COMPONENT_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <HashCode.hpp>

namespace hyperion {

class Node;

struct NodeLinkComponent
{
    Weak<Node>  node;

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        return HashCode();
    }
};

static_assert(sizeof(NodeLinkComponent) == 8, "NodeLinkComponent must be 8 bytes");

} // namespace hyperion

#endif