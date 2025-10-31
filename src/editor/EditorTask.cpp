/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorTask.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <EditorTask.generated.inl>

namespace hyperion {

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
    ThreadBase* gameThread = Threads::GetThread(g_gameThread);
    Assert(gameThread != nullptr);

    m_task = gameThread->GetScheduler().Enqueue([this, weakThis = WeakHandleFromThis()]()
        {
            Handle<TickableEditorTask> task = weakThis.Lock();

            if (!task)
            {
                HYP_LOG(Editor, Warning, "EditorTask was destroyed before it could be processed");
            }

            m_isCommitted.Set(true, MemoryOrder::RELEASE);

            Process();
        });

    return true;
}

void TickableEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted())
    {
        if (!Threads::IsOnThread(g_gameThread))
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
    return m_task.IsCompleted();
}

#pragma endregion TickableEditorTask

#pragma region LongRunningEditorTask

LongRunningEditorTask::LongRunningEditorTask()
    : m_isCommitted(false)
{
}

LongRunningEditorTask::~LongRunningEditorTask()
{
    if (m_thread && m_thread->IsRunning())
    {
        m_thread->Stop();
        m_thread->Join();

        m_thread.Reset();
    }
}

bool LongRunningEditorTask::Commit()
{
    m_thread = MakePimpl<EditorTaskThread>();

    if (!m_thread->Start())
    {
        HYP_LOG(Editor, Error, "Failed to start EditorTaskThread");

        return false;
    }

    m_task = m_thread->GetScheduler().Enqueue([this]()
        {
            m_isCommitted.Set(true, MemoryOrder::RELEASE);

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

    if (m_thread && m_thread->IsRunning())
    {
        m_thread->Stop();
        m_thread->Join();

        m_thread.Reset();
    }

    m_isCommitted.Set(false, MemoryOrder::RELEASE);
}

bool LongRunningEditorTask::IsCompleted_Impl() const
{
    return m_task.IsCompleted();
}

#pragma endregion LongRunningEditorTask

} // namespace hyperion
