/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorTask.hpp>
#include <editor/EditorState.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <engine/threads/SimThread.hpp>

#include <EditorTask.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

extern Handle<EditorState> g_editorState;

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
    : isComplete(false),
      m_timer(),
      m_isForegroundTask(false),
      m_isCommitted(false)
{
}

bool TickableEditorTask::Commit()
{
    isComplete = false;

    m_isCommitted.Set(true, MemoryOrder::RELEASE);

    Start();

    return true;
}

void TickableEditorTask::Cancel_Impl()
{
    if (!m_isCancellationRequested)
    {
        m_isCancellationRequested = true;

        OnCancel();

        m_isCommitted.Set(false, MemoryOrder::RELEASE);
    }
}

bool TickableEditorTask::IsCompleted_Impl() const
{
    return isComplete;
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
    m_isCancellationRequested = true;

    if (m_task.IsValid() && !m_task.IsCompleted())
    {
        if (!m_task.Cancel())
        {
            HYP_LOG(Editor, Warning, "Failed to cancel task, awaiting...");

            m_task.Await();
        }

        OnCancel();

        m_isCommitted.Set(false, MemoryOrder::RELEASE);
    }
}

bool LongRunningEditorTask::IsCompleted_Impl() const
{
    return !m_task.IsValid() || m_task.IsCompleted();
}

#pragma endregion LongRunningEditorTask

#pragma region EditorTaskScope

EditorTaskScope::EditorTaskScope(
    ConstructWithProcTag, const Class* editorTaskClass, Proc<void()>&& proc, bool isForegroundTask)
{
    Assert(editorTaskClass != nullptr);

    if (editorTaskClass->IsDerivedFrom(TickableEditorTask::StaticClass()))
    {
        BoxedValue boxed;
        if (!editorTaskClass->CreateInstance(boxed))
        {
            HYP_FAIL("Failed to create instance of editor task type '{}'", editorTaskClass->GetName());
            return;
        }

        Handle<TickableEditorTask> task = boxed.Get<Handle<TickableEditorTask>>();
        Assert(task.IsValid());

        task->m_tickProc = std::move(proc);

        task->SetIsForegroundTask(isForegroundTask);

        m_task = std::move(task);
    }
    else if (editorTaskClass->IsDerivedFrom(LongRunningEditorTask::StaticClass()))
    {
        BoxedValue boxed;
        if (!editorTaskClass->CreateInstance(boxed))
        {
            HYP_FAIL("Failed to create instance of editor task type '{}'", editorTaskClass->GetName());
            return;
        }

        Handle<LongRunningEditorTask> task = boxed.Get<Handle<LongRunningEditorTask>>();
        Assert(task.IsValid());

        task->m_processProc = std::move(proc);

        m_task = std::move(task);
    }
    else
    {
        HYP_NOT_IMPLEMENTED();
    }

    if (m_task.IsValid())
    {
        if (m_task->Commit())
        {
            g_editorState->AddTask(m_task);
        }
        else
        {
            HYP_LOG(Editor, Error, "Failed to commit editor task of type '{}'", m_task->InstanceClass()->GetName());
            m_task.Reset();
        }
    }
}

EditorTaskScope::~EditorTaskScope()
{
    if (m_task.IsValid())
    {
        if (TickableEditorTask* tickableEditorTask = ObjCast<TickableEditorTask>(m_task.Get()))
        {
            tickableEditorTask->isComplete = true;
        }
        else if (LongRunningEditorTask* longRunningEditorTask = ObjCast<LongRunningEditorTask>(m_task.Get()))
        {
            longRunningEditorTask->GetTask().Await();
        }
        else
        {
            HYP_UNREACHABLE();
        }
    }
}

#pragma endregion EditorTaskScope

} // namespace Hyperion
