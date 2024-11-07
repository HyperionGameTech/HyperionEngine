/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorDelegates.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypMember.hpp>
#include <core/object/HypClass.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

EditorDelegates::EditorDelegates()
    : m_scheduler(Threads::GetThreadID(ThreadName::THREAD_GAME))
{
}

void EditorDelegates::AddNodeWatcher(Name watcher_key, const Weak<Node> &root_node, const FlatSet<const HypProperty *> &properties_to_watch, Proc<void, Node *, const HypProperty *> &&proc)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    RC<Node> root_node_rc = root_node.Lock();

    auto it = m_node_watchers.Find(watcher_key);

    if (it == m_node_watchers.End()) {
        it = m_node_watchers.Insert(watcher_key, NodeWatcher { }).first;
    }

    for (const HypProperty *property : properties_to_watch) {
        AssertThrow(property != nullptr);
    }

    NodeWatcher &node_watcher = it->second;
    node_watcher.root_node = root_node;
    node_watcher.properties_to_watch.Merge(properties_to_watch);
    node_watcher.OnChange.Bind(std::move(proc)).Detach();
}

void EditorDelegates::RemoveNodeWatcher(WeakName watcher_key)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    auto it = m_node_watchers.FindAs(watcher_key);

    if (it == m_node_watchers.End()) {
        return;
    }

    m_node_watchers.Erase(it);
}

NodeWatcher *EditorDelegates::GetNodeWatcher(WeakName watcher_key)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    auto it = m_node_watchers.FindAs(watcher_key);

    if (it == m_node_watchers.End()) {
        return nullptr;
    }

    return &it->second;
}

void EditorDelegates::OnNodeUpdate(Node *node, const HypProperty *property)
{
    AssertThrow(property != nullptr);

    auto Impl = [this, node_weak = node->WeakRefCountedPtrFromThis(), property]()
    {
        HYP_SCOPE;
    
        RC<Node> node = node_weak.Lock();

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

            if (!node_watcher.root_node.Empty() && !node->IsOrHasParent(node_watcher.root_node.GetUnsafe())) {
                continue;
            }

            if (!node_watcher.properties_to_watch.Empty() && !node_watcher.properties_to_watch.Contains(property)) {
                continue;
            }

            node_watcher.OnChange(node, property);
        }
    };

    if (Threads::IsOnThread(ThreadName::THREAD_GAME)) {
        Impl();
    } else {
        m_scheduler.Enqueue(Impl, TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void EditorDelegates::Update()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    Queue<typename Scheduler<>::ScheduledTask> tasks;

    if (uint32 num_enqueued = m_scheduler.NumEnqueued()) {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any()) {
            tasks.Pop().Execute();
        }
    }
}

} // namespace hyperion

