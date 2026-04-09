/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/name/Name.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/FlatSet.hpp>

#include <Core/functional/Delegate.hpp>
#include <Core/functional/Proc.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/Scheduler.hpp>

#include <Core/reflection/Property.hpp>

#include <Core/math/Transform.hpp>

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

    HYP_API EditorDelegates();
    EditorDelegates(const EditorDelegates& other) = delete;
    EditorDelegates& operator=(const EditorDelegates& other) = delete;
    EditorDelegates(EditorDelegates&& other) = delete;
    EditorDelegates& operator=(EditorDelegates&& other) = delete;
    ~EditorDelegates() = default;

    /*! \brief Receive events and changes to any node that is a descendant of the given \p rootNode. */
    HYP_API void AddNodeWatcher(Name watcherKey, Node* rootNode, Span<const Property> propertiesToWatch, Proc<void(Node*, const Property*)>&& proc);
    HYP_API int RemoveNodeWatcher(StringHash watcherKey, Node* rootNode);
    HYP_API int RemoveNodeWatchers(StringHash watcherKey);

    HYP_API void OnNodeUpdate(Node* node, const Property* property);

    HYP_API void Update();

private:
    Array<Pair<Name, NodeWatcher>> m_nodeWatchers;
    HashMap<Node*, SuppressedNode> m_suppressedNodes;

    Scheduler m_scheduler;
};

} // namespace Hyperion
