/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_DELEGATES_HPP
#define HYPERION_EDITOR_DELEGATES_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatSet.hpp>

#include <core/functional/Delegate.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/Name.hpp>

#include <math/Transform.hpp>

#include <Types.hpp>

namespace hyperion {

class Node;

class HYP_API EditorDelegates
{
public:
    static EditorDelegates &GetInstance();

    void AddNodeWatcher(Name watcher_key, const FlatSet<Name> &properties_to_watch, Proc<void, Node *, Name, ConstAnyRef> &&proc);

    /*! \brief Watch this node for all changes. Use OnWatchedNodeUpdate to catch all changes. */
    void WatchNode(Node *node);

    /*! \brief Stop watching this node for all changes. */
    void UnwatchNode(Node *node);

    void OnNodeUpdate(Node *node, Name property_name, ConstAnyRef data);

private:
    struct NodeWatcher
    {
        FlatSet<Name>                               properties_to_watch;
        Delegate<void, Node *, Name, ConstAnyRef>   delegate;
    };

    FlatSet<Node *>             m_nodes;
    HashMap<Name, NodeWatcher>  m_node_watchers;
};

} // namespace hyperion

#endif

