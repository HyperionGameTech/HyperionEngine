#include <TaskSystem.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <type_traits>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    using TaskDelegate = std::add_pointer_t<void()>;

    TaskBatch *TaskBatch_Create()
    {
        return new TaskBatch();
    }

    void TaskBatch_Destroy(TaskBatch *task_batch)
    {
        delete task_batch;
    }

    bool TaskBatch_IsCompleted(const TaskBatch *task_batch)
    {
        return task_batch->IsCompleted();
    }

    UInt32 TaskBatch_NumCompleted(const TaskBatch *task_batch)
    {
        return task_batch->num_completed.Get(MemoryOrder::RELAXED);
    }

    UInt32 TaskBatch_NumEnqueued(const TaskBatch *task_batch)
    {
        return task_batch->num_enqueued;
    }

    void TaskBatch_AwaitCompletion(TaskBatch *task_batch)
    {
        task_batch->AwaitCompletion();
    }

    void TaskBatch_AddTask(TaskBatch *task_batch, TaskDelegate delegate)
    {
        task_batch->AddTask([delegate](...)
        {
            delegate();
        });
    }

    void TaskBatch_Launch(TaskBatch *task_batch)
    {
        TaskSystem::GetInstance().EnqueueBatch(task_batch);
    }
}