/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorTaskManager.hpp>

#include <editor/ui/EditorUI.hpp>

#include <core/reflection/Class.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UIText.hpp>

#include <scene/components/UIComponent.hpp>

namespace hyperion {

Handle<UIObject> RunningEditorTask::CreateUIObject(UIStage* uiStage) const
{
    Assert(uiStage != nullptr);

    Handle<UIPanel> panel = uiStage->CreateUIObject<UIPanel>(NAME("EditorTaskPanel"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::FILL }));
    panel->SetBackgroundColor(Color(0xFF0000FF)); // testing

    Handle<UIText> taskTitle = uiStage->CreateUIObject<UIText>(NAME("Task_Title"), Vec2i::Zero(), UIObjectSize(UIObjectSize::AUTO));
    taskTitle->SetTextSize(16.0f);
    taskTitle->SetText(m_task->InstanceClass()->GetName().LookupString());
    panel->AddChildUIObject(taskTitle);

    return panel;
}

EditorTaskManager::EditorTaskManager()
{
}

EditorTaskManager::~EditorTaskManager()
{
}

void EditorTaskManager::AddTask(const Handle<EditorTaskBase>& task)
{
    if (!task)
    {
        return;
    }

    RunningEditorTask& runningTask = m_tasks.EmplaceBack(task);

    OnTaskAdded(runningTask);

    // For long running tasks, enqueues the task in the scheduler
    task->Commit();
}

void EditorTaskManager::Tick(float delta)
{
    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        auto& task = it->GetTask();

        if (task->IsCommitted())
        {
            if (TickableEditorTask* tickableTask = ObjCast<TickableEditorTask>(task.Get()))
            {
                if (tickableTask->GetTimer().Waiting())
                {
                    ++it;
                    continue;
                }

                tickableTask->GetTimer().NextTick();
                tickableTask->Tick(tickableTask->GetTimer().delta);
            }

            if (task->IsCompleted())
            {
                task->OnComplete();

                OnTaskRemoved(*it);

                it = m_tasks.Erase(it);
                continue;
            }
        }

        ++it;
    }
}

} // namespace hyperion
