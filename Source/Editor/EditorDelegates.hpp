/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/FlatSet.hpp>

#include <Core/Functional/Delegate.hpp>
#include <Core/Functional/Proc.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Reflection/Property.hpp>

#include <Core/Math/Transform.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Node;
class IMember;
class Property;

struct NodeWatcher
{
    WeakHandle<Node> rootNode;
    FlatSet<const Property*> propertiesToWatch;
    Delegate<void, Node*, const Property*> OnChange;
};

class EditorDelegates
{
    struct SuppressedNode
    {
        FlatSet<const Property*> propertiesToSuppress;
        int suppressAllCounter = 0;
    };

public:
    struct SuppressUpdatesScope
    {
        SuppressUpdatesScope(EditorDelegates& editorDelegates, Node* node, const FlatSet<const Property*>& propertiesToSuppress = {})
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
                for (const Property* property : propertiesToSuppress)
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

            for (const Property* property : propertiesToSuppress)
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
        FlatSet<const Property*> propertiesToSuppress;
        bool suppressAll = false;
    };

    EDITOR_API EditorDelegates();
    EditorDelegates(const EditorDelegates& other) = delete;
    EditorDelegates& operator=(const EditorDelegates& other) = delete;
    EditorDelegates(EditorDelegates&& other) = delete;
    EditorDelegates& operator=(EditorDelegates&& other) = delete;
    ~EditorDelegates() = default;

    /*! \brief Receive events and changes to any node that is a descendant of the given \p rootNode. */
    EDITOR_API void AddNodeWatcher(Name watcherKey, Node* rootNode, Span<const Property> propertiesToWatch, Proc<void(Node*, const Property*)>&& proc);
    EDITOR_API int RemoveNodeWatcher(StringHash watcherKey, Node* rootNode);
    EDITOR_API int RemoveNodeWatchers(StringHash watcherKey);

    EDITOR_API void OnNodeUpdate(Node* node, const Property* property);

    EDITOR_API void Update();

private:
    Array<Pair<Name, NodeWatcher>> m_nodeWatchers;
    Map<Node*, SuppressedNode> m_suppressedNodes;

    Scheduler m_scheduler;
};

} // namespace Hyperion
