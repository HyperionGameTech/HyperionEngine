/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskSystem.hpp>

#include <Types.hpp>

#include <type_traits>

using namespace hyperion;

extern "C" {
using TaskDelegate = std::add_pointer_t<void()>;

HYP_EXPORT TaskBatch *TaskBatch_Create()
{
    return new TaskBatch();
}

HYP_EXPORT void TaskBatch_Destroy(TaskBatch *task_batch)
{
    delete task_batch;
}

HYP_EXPORT bool TaskBatch_IsCompleted(const TaskBatch *task_batch)
{
    return task_batch->IsCompleted();
}

HYP_EXPORT int32 TaskBatch_NumCompleted(const TaskBatch *task_batch)
{
    return task_batch->semaphore.GetValue();
}

HYP_EXPORT int32 TaskBatch_NumEnqueued(const TaskBatch *task_batch)
{
    return task_batch->num_enqueued;
}

HYP_EXPORT void TaskBatch_AwaitCompletion(TaskBatch *task_batch)
{
    task_batch->AwaitCompletion();
}

HYP_EXPORT void TaskBatch_AddTask(TaskBatch *task_batch, TaskDelegate delegate)
{
    task_batch->AddTask([delegate](...)
    {
        delegate();
    });
}

HYP_EXPORT void TaskBatch_Launch(TaskBatch *task_batch)
{
    TaskSystem::GetInstance().EnqueueBatch(task_batch);
}
} // extern "C"