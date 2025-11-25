/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/EditorTask.hpp>

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <core/reflection/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

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

class HYP_API EditorTaskManager
{
public:
    EditorTaskManager();
    ~EditorTaskManager();

    uint32 NumRunningTasks() const
    {
        return uint32(m_tasks.Size());
    }

    void AddTask(const Handle<EditorTaskBase>& task);

    void Tick(float delta);

    Delegate<void, RunningEditorTask&> OnTaskAdded;
    Delegate<void, RunningEditorTask&> OnTaskRemoved;

private:
    Array<RunningEditorTask> m_tasks;
};

} // namespace hyperion
