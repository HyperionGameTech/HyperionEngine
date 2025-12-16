/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/EditorDelegates.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <core/threading/Threads.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorDelegates::EditorDelegates()
    : m_scheduler(g_gameThread)
{
}

void EditorDelegates::AddNodeWatcher(Name watcherKey, Node* rootNode, Span<const Property> propertiesToWatch, Proc<void(Node*, const Property*)>&& proc)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    Assert(rootNode != nullptr);

    NodeWatcher& nodeWatcher = m_nodeWatchers.EmplaceBack(watcherKey, NodeWatcher {}).second;
    nodeWatcher.rootNode = MakeWeakRef(rootNode);
    nodeWatcher.OnChange.BindThreaded(std::move(proc), g_gameThread).Detach();

    for (const Property& property : propertiesToWatch)
    {
        nodeWatcher.propertiesToWatch.Insert(&property);
    }
}

int EditorDelegates::RemoveNodeWatcher(StringHash watcherKey, Node* rootNode)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    Assert(rootNode != nullptr);

    int numRemoved = 0;

    for (auto it = m_nodeWatchers.Begin(); it != m_nodeWatchers.End(); ++it)
    {
        if (it->first == watcherKey && it->second.rootNode.GetUnsafe() == rootNode)
        {
            m_nodeWatchers.Erase(it);
            ++numRemoved;
        }
    }

    return numRemoved;
}

int EditorDelegates::RemoveNodeWatchers(StringHash watcherKey)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    int numRemoved = 0;

    for (auto it = m_nodeWatchers.Begin(); it != m_nodeWatchers.End();)
    {
        if (it->first == watcherKey)
        {
            it = m_nodeWatchers.Erase(it);

            ++numRemoved;

            continue;
        }

        ++it;
    }

    return numRemoved;
}

void EditorDelegates::OnNodeUpdate(Node* node, const Property* property)
{
    Assert(node != nullptr);
    Assert(property != nullptr);

    auto impl = [this, nodeWeak = MakeWeakRef(node), property]()
    {
        HYP_SCOPE;

        Handle<Node> node = nodeWeak.Lock();

        if (!node)
        {
            HYP_LOG(Editor, Error, "Node is no longer valid");

            return;
        }

        auto suppressedNodesIt = m_suppressedNodes.Find(node);

        if (suppressedNodesIt != m_suppressedNodes.End())
        {
            if (suppressedNodesIt->second.suppressAllCounter > 0 || suppressedNodesIt->second.propertiesToSuppress.Contains(property))
            {
                return;
            }
        }

        for (Pair<Name, NodeWatcher>& it : m_nodeWatchers)
        {
            NodeWatcher& nodeWatcher = it.second;

            if (nodeWatcher.rootNode.IsValid() && !node->IsOrHasParent(nodeWatcher.rootNode.GetUnsafe()))
            {
                continue;
            }

            if (nodeWatcher.propertiesToWatch.Any() && !nodeWatcher.propertiesToWatch.Contains(property))
            {
                continue;
            }

            nodeWatcher.OnChange(node, property);
        }
    };

    if (IsOnThread(g_gameThread))
    {
        impl();
    }
    else
    {
        m_scheduler.Enqueue(
            HYP_STATIC_MESSAGE(HYP_FUNCTION_NAME_LIT),
            std::move(impl),
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void EditorDelegates::Update()
{
    HYP_SCOPE;

    AssertOnThread(g_gameThread);

    Queue<Scheduler::ScheduledTask> tasks;

    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }
}

} // namespace hyperion
