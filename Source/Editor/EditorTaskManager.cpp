/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorTaskManager.hpp>

#include <UI/UIObject.hpp>
#include <UI/UIPanel.hpp>
#include <UI/UIText.hpp>

#include <Scene/Components/UIComponent.hpp>

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

        auto it = m_tasks.FindIf([&task](const RunningEditorTask& runningTask)
            {
                return runningTask.GetTask() == task;
            });

        if (it != m_tasks.End())
        {
            return;
        }

        m_tasks.EmplaceBack(task);
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

    for (size_t index = 0; index < m_tasks.Size();)
    {
        // Needs strong reference!
        Handle<EditorTaskBase> task = m_tasks[index].GetTask();
        Assert(task.IsValid());

        if (task->IsCancellationRequested())
        {
            m_taskProgressValues.Erase(task->Id());

            OnTaskRemoved(task);

            m_tasks.Erase(m_tasks.Begin() + index);

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
                ++index;
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

            m_tasks.Erase(m_tasks.Begin() + index);

            continue;
        }

        ++index;
    }
}

} // namespace Hyperion
