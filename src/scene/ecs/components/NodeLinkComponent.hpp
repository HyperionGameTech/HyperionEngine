#ifndef HYPERION_V2_ECS_NODE_LINK_COMPONENT_HPP
#define HYPERION_V2_ECS_NODE_LINK_COMPONENT_HPP

#include <core/lib/RefCountedPtr.hpp>

#include <HashCode.hpp>

namespace hyperion::v2 {

class Node;

struct NodeLinkComponent
{
    Weak<Node>  node;
};

static_assert(sizeof(NodeLinkComponent) == 8, "NodeLinkComponent must be 8 bytes");

} // namespace hyperion::v2

#endif