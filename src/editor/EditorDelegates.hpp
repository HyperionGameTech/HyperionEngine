/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/Name.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatSet.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/object/HypProperty.hpp>

#include <scene/Node.hpp>

#include <core/math/Transform.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Node;
class IHypMember;
class HypProperty;

struct NodeWatcher
{
    WeakHandle<Node> rootNode;
    FlatSet<const HypProperty*> propertiesToWatch;
    Delegate<void, Node*, const HypProperty*> OnChange;
};

class EditorDelegates
{
    struct SuppressedNode
    {
        FlatSet<const HypProperty*> propertiesToSuppress;
        int suppressAllCounter = 0;
    };

public:
    struct SuppressUpdatesScope
    {
        SuppressUpdatesScope(EditorDelegates& editorDelegates, Node* node, const FlatSet<const HypProperty*>& propertiesToSuppress = {})
            : editorDelegates(editorDelegates),
              node(node)
        {
            SuppressedNode& suppressedNode = editorDelegates.m_suppressedNodes[node];

            if (propertiesToSuppress.Empty())
            {
                ++suppressedNode.suppressAllCounter;
                suppressAll = true;
            }
            else
            {
                for (const HypProperty* property : propertiesToSuppress)
                {
                    if (!suppressedNode.propertiesToSuppress.Contains(property))
                    {
                        this->propertiesToSuppress.Insert(property);
                    }
                }
            }
        }

        ~SuppressUpdatesScope()
        {
            SuppressedNode& suppressedNode = editorDelegates.m_suppressedNodes[node];

            if (suppressAll)
            {
                --suppressedNode.suppressAllCounter;
            }

            for (const HypProperty* property : propertiesToSuppress)
            {
                suppressedNode.propertiesToSuppress.Erase(property);
            }

            if (suppressedNode.propertiesToSuppress.Empty() && suppressAll == 0)
            {
                editorDelegates.m_suppressedNodes.Erase(node);
            }
        }

        EditorDelegates& editorDelegates;
        Node* node = nullptr;
        FlatSet<const HypProperty*> propertiesToSuppress;
        bool suppressAll = false;
    };

    HYP_API EditorDelegates();
    EditorDelegates(const EditorDelegates& other) = delete;
    EditorDelegates& operator=(const EditorDelegates& other) = delete;
    EditorDelegates(EditorDelegates&& other) = delete;
    EditorDelegates& operator=(EditorDelegates&& other) = delete;
    ~EditorDelegates() = default;

    /*! \brief Receive events and changes to any node that is a descendant of the given \ref{rootNode}. */
    HYP_API void AddNodeWatcher(Name watcherKey, Node* rootNode, Span<const HypProperty> propertiesToWatch, Proc<void(Node*, const HypProperty*)>&& proc);
    HYP_API int RemoveNodeWatcher(WeakName watcherKey, Node* rootNode);
    HYP_API int RemoveNodeWatchers(WeakName watcherKey);

    HYP_API void OnNodeUpdate(Node* node, const HypProperty* property);

    HYP_API void Update();

private:
    Array<Pair<Name, NodeWatcher>> m_nodeWatchers;
    HashMap<Node*, SuppressedNode> m_suppressedNodes;

    Scheduler m_scheduler;
};

} // namespace hyperion
