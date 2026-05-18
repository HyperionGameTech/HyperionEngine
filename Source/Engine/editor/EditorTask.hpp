/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/name/Name.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/threading/Thread.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/AtomicVar.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/utilities/ClockTimer.hpp>

namespace Hyperion {

class UIObject;
class EditorTaskThread;

HYP_CLASS(Abstract)
class EditorTaskBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorTaskBase);

    friend class EditorTaskScope;

public:
    virtual ~EditorTaskBase() = default;

    HYP_METHOD()
    const String& GetTitle() const
    {
        return m_title;
    }

    HYP_METHOD()
    void SetTitle(const String& title)
    {
        m_title = title;
    }

    HYP_METHOD()
    const String& GetDescription() const
    {
        Mutex::Guard guard(m_mutex);
        return m_description;
    }

    HYP_METHOD()
    void SetDescription(const String& description)
    {
        {
            Mutex::Guard guard(m_mutex);
            m_description = description;
        }

        OnDescriptionChange();
    }

    HYP_METHOD()
    float GetProgress() const
    {
        Mutex::Guard guard(m_mutex);
        return m_progress;
    }

    HYP_METHOD()
    void SetProgress(float progress)
    {
        Mutex::Guard guard(m_mutex);
        m_progress = progress;
    }

    HYP_METHOD()
    bool IsCancellationRequested() const
    {
        Mutex::Guard guard(m_mutex);
        return m_isCancellationRequested;
    }

    HYP_METHOD()
    bool IsCommitted() const
    {
        Mutex::Guard guard(m_mutex);
        return m_isCommitted;
    }

    HYP_METHOD()
    virtual void Start() = 0;

    HYP_METHOD()
    virtual void Cancel() = 0;

    HYP_METHOD()
    virtual bool IsCompleted() const = 0;

    HYP_METHOD()
    virtual bool Commit() = 0;

    HYP_FIELD()
    ScriptableDelegate<void> OnComplete;

    HYP_FIELD()
    ScriptableDelegate<void> OnCancel;

    HYP_FIELD()
    ScriptableDelegate<void> OnDescriptionChange;

protected:
    EditorTaskBase()
        : m_progress(0.0f),
          m_isCancellationRequested(false),
          m_isCommitted(false)
    {
    }
    
    String m_title;
    String m_description;
    float m_progress;
    bool m_isCancellationRequested;
    bool m_isCommitted;
    mutable Mutex m_mutex;
};

HYP_CLASS(Description = "A task that runs on the sim thread and is has Process() called every tick")
class HYP_API TickableEditorTask : public EditorTaskBase
{
    HYP_OBJECT_BODY(TickableEditorTask);

    friend class EditorTaskScope;

public:
    TickableEditorTask();

    TickableEditorTask(
        const String& title,
        const String& description = "")
        : TickableEditorTask()
    {
        m_title = title;
        m_description = description;
    }

    TickableEditorTask(
        Proc<void()>&& tickProc,
        const String& title,
        const String& description = "")
        : TickableEditorTask()
    {
        m_tickProc = std::move(tickProc);
        m_title = title;
        m_description = description;
    }

    virtual ~TickableEditorTask() override = default;

    HYP_FORCE_INLINE ClockTimer& GetTimer()
    {
        return m_timer;
    }

    HYP_FORCE_INLINE const ClockTimer& GetTimer() const
    {
        return m_timer;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsForegroundTask() const
    {
        return m_isForegroundTask;
    }

    HYP_METHOD()
    void SetIsForegroundTask(bool isForeground)
    {
        m_isForegroundTask = isForeground;
    }

    HYP_METHOD(Scriptable)
    virtual void Start() override;

    HYP_METHOD(Scriptable)
    virtual void Cancel() override;

    HYP_METHOD(Scriptable)
    virtual bool IsCompleted() const override;

    HYP_METHOD()
    virtual bool Commit() override final;

    HYP_METHOD(Scriptable)
    virtual void Tick();
    
    bool isComplete;

protected:
    HYP_METHOD()
    virtual void Start_Impl()
    {
        // nothing by default
    }
    
    HYP_METHOD()
    virtual void Cancel_Impl();
    
    HYP_METHOD()
    virtual bool IsCompleted_Impl() const;
    
    HYP_METHOD()
    virtual void Tick_Impl()
    {
        // by default call tickProc if set
        AssertDebug(m_tickProc.IsValid(), "No TickProc set for default Tick impl");

        if (m_tickProc.IsValid())
        {
            m_tickProc();
        }
    }

    ClockTimer m_timer;

    bool m_isForegroundTask;

private:
    Proc<void()> m_tickProc;
};

HYP_CLASS(Description = "A task that runs on a Task thread and has Process() called one time only")
class HYP_API LongRunningEditorTask : public EditorTaskBase
{
    HYP_OBJECT_BODY(LongRunningEditorTask);

    friend class EditorTaskScope;

public:
    LongRunningEditorTask();
    
    LongRunningEditorTask(
        const String& title,
        const String& description = "")
        : LongRunningEditorTask()
    {
        m_title = title;
        m_description = description;
    }

    LongRunningEditorTask(
        Proc<void()>&& processProc,
        const String& title,
        const String& description = "")
        : LongRunningEditorTask()
    {
        m_processProc = std::move(processProc);
        m_title = title;
        m_description = description;
    }

    virtual ~LongRunningEditorTask() override;

    HYP_FORCE_INLINE Task<void>& GetTask()
    {
        return m_task;
    }

    HYP_FORCE_INLINE const Task<void>& GetTask() const
    {
        return m_task;
    }

    HYP_METHOD(Scriptable)
    virtual void Start() override;

    HYP_METHOD(Scriptable)
    virtual void Cancel() override;

    HYP_METHOD(Scriptable)
    virtual bool IsCompleted() const override;

    HYP_METHOD(Scriptable)
    virtual void Process();

    HYP_METHOD()
    virtual bool Commit() override final;

protected:
    HYP_METHOD()
    virtual void Start_Impl()
    {
        // nothing by default
    }
    
    HYP_METHOD()
    virtual void Cancel_Impl();
    
    HYP_METHOD()
    virtual bool IsCompleted_Impl() const;
    
    HYP_METHOD()
    virtual void Process_Impl()
    {
        // by default call processProc if set
        AssertDebug(m_processProc.IsValid(), "No ProcessProc set for default Tick impl");

        if (m_processProc.IsValid())
        {
            m_processProc();
        }
    }

    Task<void> m_task;

private:
    Proc<void()> m_processProc;
};

class EditorTaskScope
{
    enum ConstructWithProcTag
    {
        ConstructWithProc
    };

    EditorTaskScope(ConstructWithProcTag,
        const Class* editorTaskClass,
        Proc<void()>&& proc,
        const String& title,
        const String& description = "",
        bool isForegroundTask = false);

public:
    EditorTaskScope()
        : m_task(Handle<EditorTaskBase>::Null())
    {
    }

    template <class TargetType>
    EditorTaskScope(
        const Class* editorTaskClass,
        TargetType* thisPtr,
        void (TargetType::*memFn)(void),
        const String& title,
        const String& description = "",
        bool isForegroundTask = false)
        : EditorTaskScope(
            ConstructWithProc,
            editorTaskClass,
            [memFn, thisPtr]() { thisPtr->*memFn(); },
            title,
            description,
            isForegroundTask)
    {
    }
    
    template <class TargetType>
    EditorTaskScope(
        const Class* editorTaskClass,
        const TargetType* thisPtr,
        void (TargetType::*memFn)(void) const,
        const String& title,
        const String& description = "",
        bool isForegroundTask = false)
        : EditorTaskScope(
            ConstructWithProc,
            editorTaskClass,
            [memFn, thisPtr]() { thisPtr->*memFn(); },
            title,
            description,
            isForegroundTask)
    {
    }
    
    template <class Functor>
    EditorTaskScope(
        const Class* editorTaskClass,
        Functor&& functor,
        const String& title,
        const String& description = "",
        bool isForegroundTask = false)
        : EditorTaskScope(
            ConstructWithProc,
            editorTaskClass,
            std::forward<Functor>(functor),
            title,
            description,
            isForegroundTask)
    {
    }

    EditorTaskScope(const EditorTaskScope& other)
        : m_task(other.m_task)
    {
    }

    EditorTaskScope& operator=(const EditorTaskScope& other)
    {
        if (this == &other || m_task == other.m_task)
        {
            return *this;
        }

        if (m_task.IsValid())
        {
            Reset();
        }

        m_task = other.m_task;

        return *this;
    }

    ~EditorTaskScope();
    
    HYP_FORCE_INLINE EditorTaskBase* GetEditorTask() const
    {
        return m_task;
    }
    
    void Reset(bool shouldCancel = false);

private:
    Handle<EditorTaskBase> m_task;
};

} // namespace Hyperion
