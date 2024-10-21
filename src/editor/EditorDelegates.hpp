/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_DELEGATES_HPP
#define HYPERION_EDITOR_DELEGATES_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatSet.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/Mutex.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/Name.hpp>

#include <math/Transform.hpp>

#include <Types.hpp>

namespace hyperion {

class Node;
class IHypMember;
struct HypProperty;

struct NodeWatcher
{
    Weak<Node>                                  root_node;
    FlatSet<const HypProperty *>                properties_to_watch;
    Delegate<void, Node *, const HypProperty *> OnChange;
};

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

    /*! \brief Receive events and changes to any node that is a descendant of the given \ref{root_node}. */
    void AddNodeWatcher(Name watcher_key, const Weak<Node> &root_node, const FlatSet<const HypProperty *> &properties_to_watch, Proc<void, Node *, const HypProperty *> &&proc);
    void RemoveNodeWatcher(WeakName watcher_key);
    NodeWatcher *GetNodeWatcher(WeakName watcher_key);

    void OnNodeUpdate(Node *node, const HypProperty *property);

private:
    HashMap<Name, NodeWatcher>  m_node_watchers;
};

} // namespace hyperion

#endif

