/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_TASK_HPP
#define HYPERION_EDITOR_TASK_HPP

#include <core/Name.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/object/HypObject.hpp>

#include <Types.hpp>

namespace hyperion {

class UIObject;

HYP_CLASS(Abstract)
class EditorTaskBase : public HypObject<EditorTaskBase>
{
    HYP_OBJECT_BODY(EditorTaskBase);

public:
    virtual ~EditorTaskBase() = default;

    virtual ThreadType GetRunnableThreadType() const = 0;

    HYP_METHOD()
    virtual bool IsCommitted() const = 0;

    HYP_METHOD()
    virtual void Cancel() = 0;

    HYP_METHOD()
    virtual bool IsCompleted() const = 0;

    HYP_METHOD()
    virtual void Process() = 0;

    HYP_METHOD()
    virtual void Commit() = 0;

    HYP_METHOD()
    virtual void Tick(float delta) = 0;

    HYP_FIELD()
    ScriptableDelegate<void> OnComplete;

    HYP_FIELD()
    ScriptableDelegate<void> OnCancel;
};

HYP_CLASS(Abstract, Description = "A task that runs on the game thread and is has Process() called every tick")
class HYP_API TickableEditorTask : public EditorTaskBase
{
    HYP_OBJECT_BODY(TickableEditorTask);

public:
    TickableEditorTask();
    virtual ~TickableEditorTask() override = default;

    virtual ThreadType GetRunnableThreadType() const override final
    {
        return ThreadType::THREAD_TYPE_GAME;
    }

    HYP_METHOD()
    virtual bool IsCommitted() const override final
    {
        return m_is_committed.Get(MemoryOrder::ACQUIRE);
    }

    HYP_METHOD(Scriptable)
    virtual void Cancel() override;

    HYP_METHOD(Scriptable)
    virtual bool IsCompleted() const override;

    HYP_METHOD(Scriptable)
    virtual void Process() override;

    HYP_METHOD()
    virtual void Commit() override final;

    HYP_METHOD(Scriptable)
    virtual void Tick(float delta) override;

protected:
    virtual void Cancel_Impl();
    virtual bool IsCompleted_Impl() const;

    virtual void Process_Impl()
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void Tick_Impl(float delta)
    {
        HYP_PURE_VIRTUAL();
    }

private:
    AtomicVar<bool> m_is_committed;
    Task<void> m_task;
};

HYP_CLASS(Abstract, Description = "A task that runs on a Task thread and has Process() called one time only")
class HYP_API LongRunningEditorTask : public EditorTaskBase
{
    HYP_OBJECT_BODY(LongRunningEditorTask);

public:
    LongRunningEditorTask();
    virtual ~LongRunningEditorTask() override = default;

    virtual ThreadType GetRunnableThreadType() const override final
    {
        return ThreadType::THREAD_TYPE_TASK;
    }

    HYP_METHOD()
    virtual bool IsCommitted() const override final
    {
        return m_is_committed.Get(MemoryOrder::ACQUIRE);
    }

    HYP_METHOD(Scriptable)
    virtual void Cancel() override;

    HYP_METHOD(Scriptable)
    virtual bool IsCompleted() const override;

    HYP_METHOD(Scriptable)
    virtual void Process() override;

    HYP_METHOD()
    virtual void Commit() override final;

    HYP_METHOD()
    virtual void Tick(float delta) override final
    {
        // Do nothing
    }

protected:
    virtual void Cancel_Impl();
    virtual bool IsCompleted_Impl() const;

    virtual void Process_Impl()
    {
        HYP_PURE_VIRTUAL();
    }

    AtomicVar<bool> m_is_committed;
    Task<void> m_task;
};

} // namespace hyperion

#endif
