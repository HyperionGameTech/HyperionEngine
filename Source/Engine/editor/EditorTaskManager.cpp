/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <editor/EditorTaskManager.hpp>

#include <editor/ui/EditorUI.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UIText.hpp>

#include <scene/components/UIComponent.hpp>

namespace Hyperion {

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
    : m_timer(1.0)
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

    {
        Mutex::Guard guard(m_mutex);
        RunningEditorTask& runningTask = m_tasks.EmplaceBack(task);
    }

    OnTaskAdded(task);
}

void EditorTaskManager::Tick()
{
    if (m_timer.Waiting())
    {
        return;
    }

    m_timer.NextTick();

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        const Handle<EditorTaskBase>& task = it->GetTask();
        
        if (task->IsCancellationRequested())
        {
            m_taskProgressValues.Erase(task->Id());

            OnTaskRemoved(task);

            it = m_tasks.Erase(it);

            continue;
        }

        if (!task->IsCommitted())
        {
            task->Commit();

            Assert(task->IsCommitted());
        }

        if (TickableEditorTask* tickableTask = DynamicCast<TickableEditorTask>(task.Get()))
        {
            if (tickableTask->GetTimer().Waiting())
            {
                ++it;
                continue;
            }

            tickableTask->GetTimer().NextTick();
            tickableTask->Tick();
        }
        
        if (task->GetProgress() != m_taskProgressValues[task->Id()])
        {
            OnTaskProgressUpdated(task);

            m_taskProgressValues[task->Id()] = task->GetProgress();
        }
        
        if (task->IsCompleted())
        {
            m_taskProgressValues.Erase(task->Id());

            task->OnComplete();

            OnTaskRemoved(task);

            it = m_tasks.Erase(it);

            continue;
        }

        ++it;
    }
}

} // namespace Hyperion
