/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_DELEGATES_HPP
#define HYPERION_EDITOR_DELEGATES_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatSet.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/Name.hpp>

#include <math/Transform.hpp>

#include <Types.hpp>

namespace hyperion {

class Node;
class IHypMember;
class HypProperty;

struct NodeWatcher
{
    Weak<Node>                                  root_node;
    FlatSet<const HypProperty *>                properties_to_watch;
    Delegate<void, Node *, const HypProperty *> OnChange;
};

class HYP_API EditorDelegates
{
    struct SuppressedNode
    {
        FlatSet<const HypProperty *>    properties_to_suppress;
        int                             suppress_all_counter = 0;
    };

public:
    struct SuppressUpdatesScope
    {
        SuppressUpdatesScope(EditorDelegates &editor_delegates, Node *node, const FlatSet<const HypProperty *> &properties_to_suppress = { })
            : editor_delegates(editor_delegates),
              node(node)
        {
            SuppressedNode &suppressed_node = editor_delegates.m_suppressed_nodes[node];

            if (properties_to_suppress.Empty()) {
                ++suppressed_node.suppress_all_counter;
                suppress_all = true;
            } else {
                for (const HypProperty *property : properties_to_suppress) {
                    if (!suppressed_node.properties_to_suppress.Contains(property)) {
                        this->properties_to_suppress.Insert(property);
                    }
                }
            }
        }

        ~SuppressUpdatesScope()
        {
            SuppressedNode &suppressed_node = editor_delegates.m_suppressed_nodes[node];

            if (suppress_all) {
                --suppressed_node.suppress_all_counter;
            }

            for (const HypProperty *property : properties_to_suppress) {
                suppressed_node.properties_to_suppress.Erase(property);
            }

            if (suppressed_node.properties_to_suppress.Empty() && suppress_all == 0) {
                editor_delegates.m_suppressed_nodes.Erase(node);
            }
        }

        EditorDelegates                 &editor_delegates;
        Node                            *node = nullptr;
        FlatSet<const HypProperty *>    properties_to_suppress;
        bool                            suppress_all = false;
    };

    EditorDelegates();
    EditorDelegates(const EditorDelegates &other)               = delete;
    EditorDelegates &operator=(const EditorDelegates &other)    = delete;
    EditorDelegates(EditorDelegates &&other)                    = delete;
    EditorDelegates &operator=(EditorDelegates &&other)         = delete;
    ~EditorDelegates()                                          = default;

    /*! \brief Receive events and changes to any node that is a descendant of the given \ref{root_node}. */
    void AddNodeWatcher(Name watcher_key, const Weak<Node> &root_node, const FlatSet<const HypProperty *> &properties_to_watch, Proc<void, Node *, const HypProperty *> &&proc);
    void RemoveNodeWatcher(WeakName watcher_key);
    NodeWatcher *GetNodeWatcher(WeakName watcher_key);

    void OnNodeUpdate(Node *node, const HypProperty *property);

    void Update();

private:
    HashMap<Name, NodeWatcher>      m_node_watchers;
    HashMap<Node *, SuppressedNode> m_suppressed_nodes;

    Scheduler                       m_scheduler;
};

} // namespace hyperion

#endif

