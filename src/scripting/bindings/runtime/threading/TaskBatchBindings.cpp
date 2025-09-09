/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskSystem.hpp>

#include <core/Types.hpp>

#include <type_traits>

using namespace hyperion;

extern "C"
{
    using TaskDelegate = std::add_pointer_t<void()>;

    HYP_EXPORT TaskBatch* TaskBatch_Create()
    {
        return new TaskBatch();
    }

    HYP_EXPORT void TaskBatch_Destroy(TaskBatch* taskBatch)
    {
        delete taskBatch;
    }

    HYP_EXPORT bool TaskBatch_IsCompleted(const TaskBatch* taskBatch)
    {
        return taskBatch->IsCompleted();
    }

    HYP_EXPORT int32 TaskBatch_NumCompleted(const TaskBatch* taskBatch)
    {
        return taskBatch->notifier.GetValue();
    }

    HYP_EXPORT int32 TaskBatch_NumEnqueued(const TaskBatch* taskBatch)
    {
        return int32(taskBatch->executors.Size());
    }

    HYP_EXPORT void TaskBatch_AwaitCompletion(TaskBatch* taskBatch)
    {
        taskBatch->AwaitCompletion();
    }

    HYP_EXPORT void TaskBatch_AddTask(TaskBatch* taskBatch, TaskDelegate delegate)
    {
        taskBatch->AddTask(delegate);
    }

    HYP_EXPORT void TaskBatch_Launch(TaskBatch* taskBatch, void (*callback)(void))
    {
        if (!taskBatch)
        {
            return;
        }

        taskBatch->OnComplete.Bind(callback).Detach();

        TaskSystem::GetInstance().EnqueueBatch(taskBatch);
    }

} // extern "C"