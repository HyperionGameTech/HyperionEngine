/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <editor/EditorPickCache.hpp>
#include <editor/EditorTaskManager.hpp>

#include <scene/Node.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/threading/Mutex.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class EditorProject;
class EditorTaskBase;
class EditorSubsystem;

HYP_CLASS()
class HYP_API EditorState : public ObjectBase
{
    HYP_OBJECT_BODY(EditorState);

public:
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
    void SetCurrentProject(const Handle<EditorProject>& project);

    HYP_METHOD()
    void AddTask(const Handle<EditorTaskBase>& task);

    HYP_METHOD()
    Handle<Node> GetClipboardNode() const;

    HYP_METHOD()
    void SetClipboardNode(const Handle<Node>& node);

    void Initialize();

    void Update(float delta);

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorProject>> OnCurrentProjectChanged;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorTaskBase>> OnTaskStarted;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorTaskBase>> OnTaskEnded;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorTaskBase>> OnTaskProgressUpdated;

private:
    Handle<EditorProject> m_currentProject;

    EditorTaskManager m_taskManager;

    EditorPickCache m_pickCache;

    WeakHandle<Node> m_clipboardNode;

    mutable Mutex m_mutex;
};

} // namespace Hyperion
