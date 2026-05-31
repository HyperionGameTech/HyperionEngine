/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Editor/EditorTask.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

class UIObject;
class UIStage;

class RunningEditorTask
{
public:
    RunningEditorTask(const Handle<EditorTaskBase>& task)
        : m_task(task)
    {
    }

    RunningEditorTask(const Handle<EditorTaskBase>& task, const Handle<UIObject>& uiObject)
        : m_task(task),
          m_uiObject(uiObject)
    {
    }

    HYP_FORCE_INLINE const Handle<EditorTaskBase>& GetTask() const
    {
        return m_task;
    }

    HYP_FORCE_INLINE const Handle<UIObject>& GetUIObject() const
    {
        return m_uiObject;
    }

    HYP_FORCE_INLINE void SetUIObject(const Handle<UIObject>& uiObject)
    {
        m_uiObject = uiObject;
    }

    Handle<UIObject> CreateUIObject(UIStage* uiStage) const;

private:
    Handle<EditorTaskBase> m_task;
    Handle<UIObject> m_uiObject;
};

class EDITOR_API EditorTaskManager
{
public:
    EditorTaskManager();
    ~EditorTaskManager();

    uint32 NumRunningTasks() const
    {
        return uint32(m_tasks.Size());
    }

    HYP_FORCE_INLINE const ClockTimer& GetTimer() const
    {
        return m_timer;
    }

    void AddTask(const Handle<EditorTaskBase>& task);

    void Tick();

    Delegate<void, Handle<EditorTaskBase>> OnTaskAdded;
    Delegate<void, Handle<EditorTaskBase>> OnTaskRemoved;
    Delegate<void, Handle<EditorTaskBase>> OnTaskProgressUpdated;

private:
    ClockTimer m_timer;

    Array<RunningEditorTask> m_tasks;
    TMap<ObjId<EditorTaskBase>, float> m_taskProgressValues;
    mutable Mutex m_mutex;
};

} // namespace Hyperion
