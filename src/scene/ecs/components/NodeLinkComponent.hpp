/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_NODE_LINK_COMPONENT_HPP
#define HYPERION_ECS_NODE_LINK_COMPONENT_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <HashCode.hpp>

namespace hyperion {

class Node;

HYP_STRUCT(Component, Size=8, Serialize=false, Editor=false)
struct NodeLinkComponent
{
    HYP_FIELD()
    Weak<Node>  node;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }
};

} // namespace hyperion

#endif