using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Hyperion
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TaskDelegate();

    /// <summary>
    ///  Represents a native (C++) TaskBatch (see TaskSystem.hpp)
    /// </summary>
    public class TaskBatch : IDisposable
    {
        private IntPtr ptr;

        public TaskBatch()
        {
            this.ptr = TaskBatch_Create();
        }

        public void Dispose()
        {
            if (ptr == IntPtr.Zero)
            {
                throw new ObjectDisposedException("TaskBatch");
            }

            if (!IsCompleted)
            {
                throw new InvalidOperationException("TaskBatch is not completed");
            }

            TaskBatch_Destroy(ptr);
        }

        public bool IsCompleted
        {
            get
            {
                return TaskBatch_IsCompleted(ptr);
            }
        }

        public uint NumCompleted
        {
            get
            {
                return TaskBatch_NumCompleted(ptr);
            }
        }

        public uint NumEnqueued
        {
            get
            {
                return TaskBatch_NumEnqueued(ptr);
            }
        }

        public void AddTask(TaskDelegate fn)
        {
            if (!IsCompleted)
            {
                throw new InvalidOperationException("TaskBatch is not completed, cannot add tasks");
            }

            TaskBatch_AddTask(ptr, fn);
        }

        async public void Execute()
        {
            TaskBatch_Launch(ptr);
            
            // Implement a simple awaiter for TaskBatch
            while (!IsCompleted)
            {
                await Task.Delay(1);
            }
        }

        public void AwaitCompletion()
        {
            TaskBatch_AwaitCompletion(ptr);
        }

        [DllImport("libhyperion", EntryPoint = "TaskBatch_Create")]
        private static extern IntPtr TaskBatch_Create();

        [DllImport("libhyperion", EntryPoint = "TaskBatch_Destroy")]
        private static extern void TaskBatch_Destroy(IntPtr taskBatchPtr);

        [DllImport("libhyperion", EntryPoint = "TaskBatch_IsCompleted")]
        private static extern bool TaskBatch_IsCompleted(IntPtr taskBatchPtr);

        [DllImport("libhyperion", EntryPoint = "TaskBatch_NumCompleted")]
        private static extern uint TaskBatch_NumCompleted(IntPtr taskBatchPtr);

        [DllImport("libhyperion", EntryPoint = "TaskBatch_NumEnqueued")]
        private static extern uint TaskBatch_NumEnqueued(IntPtr taskBatchPtr);

        [DllImport("libhyperion", EntryPoint = "TaskBatch_AddTask")]
        private static extern void TaskBatch_AddTask(IntPtr taskBatchPtr, TaskDelegate fn);

        [DllImport("libhyperion", EntryPoint = "TaskBatch_AwaitCompletion")]
        private static extern void TaskBatch_AwaitCompletion(IntPtr taskBatchPtr);

        [DllImport("libhyperion", EntryPoint = "TaskBatch_Launch")]
        private static extern void TaskBatch_Launch(IntPtr taskBatchPtr);
    }
}