/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorTask.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region TickableEditorTask

void TickableEditorTask::Commit()
{
    if (Threads::IsOnThread(ThreadName::THREAD_GAME)) {
        Process();
    } else {
        IThread *game_thread = Threads::GetThread(ThreadName::THREAD_GAME);
        AssertThrow(game_thread != nullptr);

        game_thread->GetScheduler()->Enqueue([weak_this = WeakRefCountedPtrFromThis()]()
        {
            if (RC<IEditorTask> task = weak_this.Lock().CastUnsafe<TickableEditorTask>()) {
                task->Process();
            } else {
                HYP_LOG(Editor, LogLevel::WARNING, "EditorTask was destroyed before it could be processed");
            }
        }, TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

#pragma endregion TickableEditorTask

#pragma region LongRunningEditorTask

void LongRunningEditorTask::Commit()
{
    m_task = TaskSystem::GetInstance().Enqueue([this]()
    {
        Process();
    });
}

void LongRunningEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted()) {
        if (!m_task.Cancel()) {
            m_task.Await();
        }
    }
}

bool LongRunningEditorTask::IsCompleted_Impl() const
{
    return m_task.IsCompleted();
}

#pragma endregion LongRunningEditorTask

} // namespace hyperion