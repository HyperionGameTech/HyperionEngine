/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>
#include <core/Name.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/utilities/ClockTimer.hpp>

namespace Hyperion {

class UIObject;
class EditorTaskThread;

HYP_CLASS(Abstract)
class EditorTaskBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorTaskBase);

public:
    virtual ~EditorTaskBase() = default;

    HYP_METHOD()
    float GetProgress() const
    {
        return m_progress;
    }

    HYP_METHOD()
    void SetProgress(float progress)
    {
        m_progress = progress;
    }

    HYP_METHOD()
    bool IsCancellationRequested() const
    {
        return m_isCancellationRequested;
    }

    HYP_METHOD()
    virtual bool IsCommitted() const = 0;

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

protected:
    EditorTaskBase()
        : m_progress(0.0f),
          m_isCancellationRequested(false)
    {
    }

    float m_progress;
    bool m_isCancellationRequested;
};

HYP_CLASS(Description = "A task that runs on the sim thread and is has Process() called every tick")
class HYP_API TickableEditorTask : public EditorTaskBase
{
    HYP_OBJECT_BODY(TickableEditorTask);

    friend class EditorTaskScope;

public:
    TickableEditorTask();

    explicit TickableEditorTask(Proc<void()>&& tickProc)
        : TickableEditorTask()
    {
        m_tickProc = std::move(tickProc);
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

    HYP_METHOD()
    virtual bool IsCommitted() const override final
    {
        return m_isCommitted.Get(MemoryOrder::ACQUIRE);
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
    AtomicVar<bool> m_isCommitted;
    Proc<void()> m_tickProc;
};

HYP_CLASS(Description = "A task that runs on a Task thread and has Process() called one time only")
class HYP_API LongRunningEditorTask : public EditorTaskBase
{
    HYP_OBJECT_BODY(LongRunningEditorTask);

    friend class EditorTaskScope;

public:
    LongRunningEditorTask();

    explicit LongRunningEditorTask(Proc<void()>&& processProc)
        : LongRunningEditorTask()
    {
        m_processProc = std::move(processProc);
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

    HYP_METHOD()
    virtual bool IsCommitted() const override final
    {
        return m_isCommitted.Get(MemoryOrder::ACQUIRE);
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

    AtomicVar<bool> m_isCommitted;
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

    EditorTaskScope(ConstructWithProcTag, const Class* editorTaskClass, Proc<void()>&& proc, bool isForegroundTask = false);

public:
    template <class TargetType>
    EditorTaskScope(const Class* editorTaskClass, TargetType* thisPtr, void (TargetType::*memFn)(void), bool isForegroundTask = false)
        : EditorTaskScope(
            ConstructWithProc,
            editorTaskClass,
            [memFn, thisPtr]() { thisPtr->*memFn(); },
            isForegroundTask)
    {
    }
    
    template <class TargetType>
    EditorTaskScope(const Class* editorTaskClass, const TargetType* thisPtr, void (TargetType::*memFn)(void) const, bool isForegroundTask = false)
        : EditorTaskScope(
            ConstructWithProc,
            editorTaskClass,
            [memFn, thisPtr]() { thisPtr->*memFn(); },
            isForegroundTask)
    {
    }
    
    template <class Functor>
    EditorTaskScope(const Class* editorTaskClass, Functor&& functor, bool isForegroundTask = false)
        : EditorTaskScope(
            ConstructWithProc,
            editorTaskClass,
            std::forward<Functor>(functor),
            isForegroundTask)
    {
    }

    EditorTaskScope(const EditorTaskScope& other) = delete;
    EditorTaskScope& operator=(const EditorTaskScope& other) = delete;

    ~EditorTaskScope();
    
    HYP_FORCE_INLINE EditorTaskBase* GetEditorTask() const
    {
        return m_task;
    }

private:
    Handle<EditorTaskBase> m_task;
};

} // namespace Hyperion
