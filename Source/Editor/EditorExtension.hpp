/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Name/Name.hpp>

#include <Plugins/PluginContext.hpp>

#include <Editor/EditorMemory.hpp>
#include <Editor/Gizmo/EditorGizmoBase.hpp>

namespace Hyperion {

class EditorSubsystem;
class DebugDrawCommandList;
class Node;
struct Ray;
class RayTestResults;
struct MouseEvent;
struct KeyboardEvent;

// editor behaviour a module adds; one instance per registration, attached to the editor subsystem while it runs
HYP_CLASS(Abstract, Serialize = false)
class EDITOR_API EditorExtension : public ObjectBase
{
    HYP_OBJECT_BODY(EditorExtension);

public:
    ~EditorExtension() override = default;

    HYP_FORCE_INLINE EditorSubsystem* GetEditorSubsystem() const
    {
        return m_editorSubsystem;
    }

    virtual void OnAttached()
    {
    }

    virtual void OnDetached()
    {
    }

    virtual void Update(float delta)
    {
    }

    virtual void DebugDraw(DebugDrawCommandList& debugDrawCommandList)
    {
    }

    // hits added here rank with the viewports' surface hits
    virtual bool TestPickRay(const Ray& ray, RayTestResults& outResults)
    {
        return false;
    }

    // a clicked node the selection takes as is, rather than raising it to its top-level parent
    virtual bool SelectsPickedNode(const Node* node) const
    {
        return false;
    }

    // viewport input the extension consumes by returning true; a click comes before the editor's own picking
    virtual bool OnViewportClick(const MouseEvent& event, const Ray& ray)
    {
        return false;
    }

    virtual bool OnViewportMouseDown(const MouseEvent& event)
    {
        return false;
    }

    virtual bool OnViewportMouseDrag(const MouseEvent& event)
    {
        return false;
    }

    virtual void OnViewportMouseMove(const MouseEvent& event)
    {
    }

    virtual bool OnViewportKeyDown(const KeyboardEvent& event)
    {
        return false;
    }

    // the mouse buttons were released, over the viewport or not
    virtual void EndViewportDrag()
    {
    }

    // a viewport tool of the extension's is on; other tools and modes call ExitTool before they take over
    virtual bool IsToolActive() const
    {
        return false;
    }

    virtual void ExitTool()
    {
    }

    virtual EditorManipulationMode ResolveManipulationMode(EditorManipulationMode requestedMode)
    {
        return requestedMode;
    }

    // returning true makes outTargetNodes the gizmo's targets instead of the selection
    virtual bool GetGizmoTargetNodes(Array<Handle<Node>>& outTargetNodes) const
    {
        return false;
    }

    virtual void OnFocusedNodeChanged(const Handle<Node>& focusedNode)
    {
    }

private:
    friend class EditorExtensionRegistry;

    EditorSubsystem* m_editorSubsystem = nullptr;
};

class EDITOR_API EditorExtensionRegistry
{
public:
    static EditorExtensionRegistry& GetInstance();

    template <class ExtensionType>
    uint32 Register()
    {
        return Register(ExtensionType::StaticClass());
    }

    uint32 Register(const Class* extensionClass);
    void Unregister(uint32 id);

    // makes and attaches an instance of every registered class; ones registered later attach as they come
    void Attach(EditorSubsystem* editorSubsystem);
    void Detach();

    Array<Handle<EditorExtension>> GetExtensions() const;

private:
    struct Entry
    {
        uint32 id = 0;
        const Class* extensionClass = nullptr;
        Handle<EditorExtension> instance;
    };

    void AttachEntry(Entry& entry);
    void SyncSubsystem();

    mutable Mutex m_mutex;
    Array<Entry> m_entries;
    uint32 m_nextId = 1;
    EditorSubsystem* m_editorSubsystem = nullptr;
};

template <class ExtensionType>
void RegisterEditorExtension(PluginContext& context)
{
    const uint32 id = EditorExtensionRegistry::GetInstance().Register<ExtensionType>();

    context.AddCleanup([id]()
        {
            EditorExtensionRegistry::GetInstance().Unregister(id);
        });
}

} // namespace Hyperion
