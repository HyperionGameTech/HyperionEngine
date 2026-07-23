/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Editor/EditorPickCache.hpp>
#include <Editor/EditorTaskManager.hpp>
#include <Editor/EditorMemory.hpp>

#include <Scene/Node.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Types.hpp>

#include <Scripting/ScriptableDelegate.hpp>


namespace Hyperion {

class EditorProject;
class EditorTaskBase;
class EditorSubsystem;

HYP_CLASS()
class EDITOR_API EditorState : public ObjectBase
{
    HYP_OBJECT_BODY(EditorState);

public:
    static Pool* GetAllocator() { return g_editorPool; }

    HYP_METHOD()
    static const Handle<EditorState>& GetInstance();

    EditorState();
    ~EditorState() override;

    HYP_FORCE_INLINE EditorPickCache& GetPickCache()
    {
        return m_pickCache;
    }

    HYP_FORCE_INLINE const EditorPickCache& GetPickCache() const
    {
        return m_pickCache;
    }

    Handle<EditorSubsystem> GetEditorSubsystem() const;

    HYP_METHOD()
    Handle<EditorProject> GetCurrentProject() const;

    HYP_METHOD()
    void SetCurrentProject(const Handle<EditorProject>& project, bool isSimulationStateChange);

    HYP_METHOD()
    void AddTask(const Handle<EditorTaskBase>& task);

    HYP_METHOD()
    Array<Handle<Node>> GetClipboardNodes() const;

    HYP_METHOD()
    void SetClipboardNodes(const Array<Handle<Node>>& nodes);

    void Initialize();

    void Update(float delta);

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorProject>, bool /* isSimulationStateChange */ > OnCurrentProjectChanged;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorTaskBase>> OnTaskStarted;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorTaskBase>> OnTaskEnded;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorTaskBase>> OnTaskProgressUpdated;

    HYP_FIELD()
    ScriptableDelegate<void> OnClipboardChanged;

private:
    Handle<EditorProject> m_currentProject;

    EditorTaskManager m_taskManager;

    EditorPickCache m_pickCache;

    Array<Handle<Node>> m_clipboardNodes;

    mutable Mutex m_mutex;
};

} // namespace Hyperion
