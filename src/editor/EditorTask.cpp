/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorTask.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <engine/threads/SimThread.hpp>

#include <EditorTask.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorTaskThread

class HYP_API EditorTaskThread final : public TaskThread
{
public:
    EditorTaskThread()
        : TaskThread(Name::Unique("EditorTaskThread"))
    {
    }

    virtual ~EditorTaskThread() override = default;
};

#pragma endregion EditorTaskThread

#pragma region TickableEditorTask

TickableEditorTask::TickableEditorTask()
    : m_isCommitted(false),
      m_timer(1.0f)
{
}

bool TickableEditorTask::Commit()
{
    m_isCommitted.Set(true, MemoryOrder::RELEASE);

    Start();

    return true;
}

void TickableEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted())
    {
        if (!IsOnThread(g_simThread))
        {
            HYP_LOG(Editor, Info, "Awaiting TickableEditorTask completion");

            m_task.Await();
        }
        else
        {
            HYP_LOG(Editor, Info, "Executing TickableEditorTask inline");

            Assert(m_task.Cancel());

            auto* promise = m_task.Promise();

            promise->Fulfill();
        }

        OnCancel();
    }

    m_isCommitted.Set(false, MemoryOrder::RELEASE);
}

bool TickableEditorTask::IsCompleted_Impl() const
{
    return !m_task.IsValid() || m_task.IsCompleted();
}

#pragma endregion TickableEditorTask

#pragma region LongRunningEditorTask

LongRunningEditorTask::LongRunningEditorTask()
    : m_isCommitted(false)
{
}

LongRunningEditorTask::~LongRunningEditorTask()
{
}

bool LongRunningEditorTask::Commit()
{
    m_task = TaskSystem::GetInstance().Enqueue([this]()
        {
            m_isCommitted.Set(true, MemoryOrder::RELEASE);

            Start();

            Process();
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND);

    return true;
}

void LongRunningEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted())
    {
        if (!m_task.Cancel())
        {
            HYP_LOG(Editor, Warning, "Failed to cancel task, awaiting...");

            m_task.Await();
        }

        OnCancel();
    }

    m_isCommitted.Set(false, MemoryOrder::RELEASE);
}

bool LongRunningEditorTask::IsCompleted_Impl() const
{
    return !m_task.IsValid() || m_task.IsCompleted();
}

#pragma endregion LongRunningEditorTask

} // namespace Hyperion
