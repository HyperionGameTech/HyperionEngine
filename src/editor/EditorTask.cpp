/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorTask.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region TickableEditorTask

TickableEditorTask::TickableEditorTask()
    : m_is_committed(false)
{
}

void TickableEditorTask::Commit()
{
    IThread *game_thread = Threads::GetThread(g_game_thread);
    AssertThrow(game_thread != nullptr);

    m_task = game_thread->GetScheduler().Enqueue([weak_this = WeakRefCountedPtrFromThis()]()
    {
        if (RC<TickableEditorTask> task = weak_this.Lock().CastUnsafe<TickableEditorTask>()) {
            task->m_is_committed.Set(true, MemoryOrder::RELEASE);

            task->Process();
        } else {
            HYP_LOG(Editor, Warning, "EditorTask was destroyed before it could be processed");
        }
    });
}

void TickableEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted()) {
        if (!Threads::IsOnThread(g_game_thread)) {
            HYP_LOG(Editor, Info, "Awaiting TickableEditorTask completion before destructing");

            m_task.Await();
        } else {
            HYP_LOG(Editor, Info, "Executing TickableEditorTask inline before destructing");

            AssertThrow(m_task.Cancel());

            auto *executor = m_task.Initialize();
            AssertThrow(executor != nullptr);

            executor->Fulfill();
        }
    }

    m_is_committed.Set(false, MemoryOrder::RELEASE);
}

bool TickableEditorTask::IsCompleted_Impl() const
{
    return m_task.IsCompleted();
}

#pragma endregion TickableEditorTask

#pragma region LongRunningEditorTask

LongRunningEditorTask::LongRunningEditorTask()
    : m_is_committed(false)
{
}

void LongRunningEditorTask::Commit()
{
    m_task = TaskSystem::GetInstance().Enqueue([this]()
    {
        m_is_committed.Set(true, MemoryOrder::RELEASE);

        Process();
    }, TaskThreadPoolName::THREAD_POOL_BACKGROUND);
}

void LongRunningEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted()) {
        if (!m_task.Cancel()) {
            HYP_LOG(Editor, Warning, "Failed to cancel task, awaiting...");

            m_task.Await();
        }
    }

    m_is_committed.Set(false, MemoryOrder::RELEASE);
}

bool LongRunningEditorTask::IsCompleted_Impl() const
{
    return m_task.IsCompleted();
}

#pragma endregion LongRunningEditorTask

} // namespace hyperion