/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorTask.hpp>
#include <Editor/EditorState.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Framework/Threads/SimThread.hpp>

#include <DotNET/ManagedMethodUtil.hpp>

#include <EditorTask.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

extern Handle<EditorState> g_editorState;

#pragma region TickableEditorTask

TickableEditorTask::TickableEditorTask()
    : isComplete(false),
      m_timer(),
      m_isForegroundTask(false)
{
}

void TickableEditorTask::Start()
{
    if (TryInvokeManagedOverrideVoid(this, "Start"))
    {
        return;
    }

    Start_Impl();
}

void TickableEditorTask::Cancel()
{
    if (TryInvokeManagedOverrideVoid(this, "Cancel"))
    {
        return;
    }

    Cancel_Impl();
}

bool TickableEditorTask::IsCompleted() const
{
    if (Optional<bool> result = TryInvokeManagedOverride<bool>(this, "IsCompleted"))
    {
        return *result;
    }

    return IsCompleted_Impl();
}

void TickableEditorTask::Tick()
{
    if (TryInvokeManagedOverrideVoid(this, "Tick"))
    {
        return;
    }

    Tick_Impl();
}

bool TickableEditorTask::Commit()
{
    {
        Mutex::Guard guard(m_mutex);

        AssertDebug(!isComplete && !m_isCommitted);

        if (isComplete || m_isCommitted)
        {
            return false;
        }

        m_isCommitted = true;
    }

    Start();

    g_editorState->AddTask(MakeStrongRef(this));

    return true;
}

void TickableEditorTask::Cancel_Impl()
{
    {
        Mutex::Guard guard(m_mutex);

        if (isComplete || !m_isCommitted)
        {
            return;
        }

        if (!m_isCancellationRequested)
        {
            m_isCancellationRequested = true;

            m_isCommitted = false;
        }
        else
        {
            return;
        }
    }

    OnCancel();
}

bool TickableEditorTask::IsCompleted_Impl() const
{
    Mutex::Guard guard(m_mutex);
    return isComplete;
}

#pragma endregion TickableEditorTask

#pragma region LongRunningEditorTask

LongRunningEditorTask::LongRunningEditorTask()
{
}

LongRunningEditorTask::~LongRunningEditorTask()
{
}

void LongRunningEditorTask::Start()
{
    if (TryInvokeManagedOverrideVoid(this, "Start"))
    {
        return;
    }

    Start_Impl();
}

void LongRunningEditorTask::Cancel()
{
    if (TryInvokeManagedOverrideVoid(this, "Cancel"))
    {
        return;
    }

    Cancel_Impl();
}

bool LongRunningEditorTask::IsCompleted() const
{
    if (Optional<bool> result = TryInvokeManagedOverride<bool>(this, "IsCompleted"))
    {
        return *result;
    }

    return IsCompleted_Impl();
}

void LongRunningEditorTask::Process()
{
    if (TryInvokeManagedOverrideVoid(this, "Process"))
    {
        return;
    }

    Process_Impl();
}

bool LongRunningEditorTask::Commit()
{
    Mutex::Guard guard(m_mutex);

    AssertDebug(!m_isCommitted);

    if (m_isCommitted)
    {
        return false;
    }

    m_isCommitted = true;

    m_task = TaskSystem::GetInstance().Enqueue([this]()
        {
            Start();

            Process();
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND);

    g_editorState->AddTask(MakeStrongRef(this));

    return true;
}

void LongRunningEditorTask::Cancel_Impl()
{
    Mutex::Guard guard(m_mutex);
    m_isCancellationRequested = true;

    if (m_task.IsValid() && !m_task.IsCompleted())
    {
        if (!m_task.Cancel())
        {
            HYP_LOG(Editor, Warning, "Failed to cancel task, awaiting...");

            m_task.Await();
        }

        OnCancel();

        m_isCommitted = false;
    }
}

bool LongRunningEditorTask::IsCompleted_Impl() const
{
    return !m_task.IsValid() || m_task.IsCompleted();
}

#pragma endregion LongRunningEditorTask

#pragma region EditorTaskScope

EditorTaskScope::EditorTaskScope(
    ConstructWithProcTag,
    const Class* editorTaskClass,
    Proc<void()>&& proc,
    const String& title,
    const String& description,
    bool isForegroundTask)
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
        m_task->m_title = title;
        m_task->m_description = description;

        if (!m_task->Commit())
        {
            HYP_LOG(Editor, Error, "Failed to commit editor task of type '{}'", m_task->InstanceClass()->GetName());
            m_task.Reset();
        }
    }
}

EditorTaskScope::~EditorTaskScope()
{
    Reset(/* shouldCancel */ false);
}

void EditorTaskScope::Reset(bool shouldCancel)
{
    if (m_task.IsValid())
    {
        /*if (shouldCancel)
        {
            m_task->Cancel();
            m_task.Reset();

            return;
        }*/

        if (TickableEditorTask* tickableEditorTask = DynamicCast<TickableEditorTask>(m_task.Get()))
        {
            Mutex::Guard guard(tickableEditorTask->m_mutex);
            tickableEditorTask->isComplete = true;
        }
        else if (LongRunningEditorTask* longRunningEditorTask = DynamicCast<LongRunningEditorTask>(m_task.Get()))
        {
            longRunningEditorTask->GetTask().Await();
        }
        else
        {
            HYP_UNREACHABLE();
        }

        m_task.Reset();
    }
}

#pragma endregion EditorTaskScope

} // namespace Hyperion
