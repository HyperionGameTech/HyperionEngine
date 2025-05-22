/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorDelegates.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

EditorDelegates::EditorDelegates()
    : m_scheduler(g_game_thread)
{
}

void EditorDelegates::AddNodeWatcher(Name watcher_key, Node *root_node, Span<const HypProperty> properties_to_watch, Proc<void(Node *, const HypProperty *)> &&proc)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(root_node != nullptr);

    NodeWatcher &node_watcher = m_node_watchers.EmplaceBack(watcher_key, NodeWatcher { }).second;
    node_watcher.root_node = root_node->WeakHandleFromThis();
    node_watcher.OnChange.Bind(std::move(proc), g_game_thread).Detach();

    for (const HypProperty &property : properties_to_watch) {
        node_watcher.properties_to_watch.Insert(&property);
    }
}

int EditorDelegates::RemoveNodeWatchers(WeakName watcher_key)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    int num_removed = 0;

    for (auto it = m_node_watchers.Begin(); it != m_node_watchers.End();) {
        if (it->first == watcher_key) {
            it = m_node_watchers.Erase(it);

            ++num_removed;

            continue;
        }

        ++it;
    }

    return num_removed;
}

void EditorDelegates::OnNodeUpdate(Node *node, const HypProperty *property)
{
    AssertThrow(node != nullptr);
    AssertThrow(property != nullptr);

    auto Impl = [this, node_weak = node->WeakHandleFromThis(), property]()
    {
        HYP_SCOPE;
    
        Handle<Node> node = node_weak.Lock();

        if (!node) {
            return;
        }

        auto suppressed_nodes_it = m_suppressed_nodes.Find(node);

        if (suppressed_nodes_it != m_suppressed_nodes.End()) {
            if (suppressed_nodes_it->second.suppress_all_counter > 0 || suppressed_nodes_it->second.properties_to_suppress.Contains(property)) {
                return;
            }
        }

        for (Pair<Name, NodeWatcher> &it : m_node_watchers) {
            NodeWatcher &node_watcher = it.second;

            if (node_watcher.root_node.IsValid() && !node->IsOrHasParent(node_watcher.root_node.GetUnsafe())) {
                continue;
            }

            if (node_watcher.properties_to_watch.Any() && !node_watcher.properties_to_watch.Contains(property)) {
                continue;
            }

            node_watcher.OnChange(node, property);
        }
    };

    if (Threads::IsOnThread(g_game_thread)) {
        Impl();
    } else {
        m_scheduler.Enqueue(
            HYP_STATIC_MESSAGE(HYP_FUNCTION_NAME_LIT),
            Impl,
            TaskEnqueueFlags::FIRE_AND_FORGET
        );
    }
}

void EditorDelegates::Update()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    Queue<Scheduler::ScheduledTask> tasks;

    if (uint32 num_enqueued = m_scheduler.NumEnqueued()) {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any()) {
            tasks.Pop().Execute();
        }
    }
}

} // namespace hyperion

