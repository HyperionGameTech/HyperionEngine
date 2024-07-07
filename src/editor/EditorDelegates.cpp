/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorDelegates.hpp>

#include <core/threading/Threads.hpp>

namespace hyperion {

EditorDelegates &EditorDelegates::GetInstance()
{
    static EditorDelegates instance;

    return instance;
}

void EditorDelegates::AddNodeWatcher(Name watcher_key, const FlatSet<Name> &properties_to_watch, Proc<void, Node *, Name, ConstAnyRef> &&proc)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    auto it = m_node_watchers.Find(watcher_key);

    if (it == m_node_watchers.End()) {
        it = m_node_watchers.Insert(watcher_key, NodeWatcher { }).first;
    }

    NodeWatcher &node_watcher = it->second;
    node_watcher.properties_to_watch.Merge(properties_to_watch);
    node_watcher.delegate.Bind(std::move(proc)).Detach();
}

void EditorDelegates::WatchNode(Node *node)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    m_nodes.Insert(node);
}

void EditorDelegates::UnwatchNode(Node *node)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    m_nodes.Erase(node);
}

void EditorDelegates::OnNodeUpdate(Node *node, Name property_name, ConstAnyRef data)
{
    // Threads::AssertOnThread(ThreadName::THREAD_GAME);

    for (Pair<Name, NodeWatcher> &it : m_node_watchers) {
        NodeWatcher &node_watcher = it.second;

        if (!node_watcher.properties_to_watch.Empty() && !node_watcher.properties_to_watch.Contains(property_name)) {
            continue;
        }

        node_watcher.delegate.Broadcast(node, property_name, data);
    }
}

} // namespace hyperion

