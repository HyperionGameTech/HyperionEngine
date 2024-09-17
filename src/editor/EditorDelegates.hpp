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
class IHypMember;

class HYP_API EditorDelegates
{
public:
    static EditorDelegates &GetInstance();

    EditorDelegates()                                           = default;
    EditorDelegates(const EditorDelegates &other)               = delete;
    EditorDelegates &operator=(const EditorDelegates &other)    = delete;
    EditorDelegates(EditorDelegates &&other)                    = delete;
    EditorDelegates &operator=(EditorDelegates &&other)         = delete;
    ~EditorDelegates()                                          = default;

    void AddNodeWatcher(Name watcher_key, const FlatSet<Name> &properties_to_watch, Proc<void, Node *, ANSIStringView> &&proc);
    void RemoveNodeWatcher(Name watcher_key);

    /*! \brief Watch this node for all changes. Use OnWatchedNodeUpdate to catch all changes. */
    void WatchNode(Node *node);

    /*! \brief Stop watching this node for all changes. */
    void UnwatchNode(Node *node);

    void OnNodeUpdate(Node *node, ANSIStringView editor_property_name);

private:
    struct NodeWatcher
    {
        FlatSet<Name>                           properties_to_watch;
        Delegate<void, Node *, ANSIStringView>  OnChange;
    };

    FlatSet<Node *>             m_nodes;
    HashMap<Name, NodeWatcher>  m_node_watchers;
};

} // namespace hyperion

#endif

