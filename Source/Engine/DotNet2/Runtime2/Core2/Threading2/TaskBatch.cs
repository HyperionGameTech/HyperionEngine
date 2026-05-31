using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Hyperion
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TaskDelegate();

    internal delegate void TaskBatchOnCompleteDelegate();

    /// <summary>
    ///  Represents a native (C++) TaskBatch (see TaskSystem.hpp)
    /// </summary>
    public class TaskBatch
    {
        protected IntPtr ptr;

        public TaskBatch()
        {
            this.ptr = TaskBatch_Create();
        }

        ~TaskBatch()
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

        public int NumCompleted
        {
            get
            {
                return TaskBatch_NumCompleted(ptr);
            }
        }

        public int NumEnqueued
        {
            get
            {
                return TaskBatch_NumEnqueued(ptr);
            }
        }

        public void AddTask(TaskDelegate fn)
        {
            TaskBatch_AddTask(ptr, fn);
        }

        public Task Execute()
        {
            var completionSource = new TaskCompletionSource();

            TaskBatch_Launch(ptr, Marshal.GetFunctionPointerForDelegate(new TaskBatchOnCompleteDelegate(() =>
            {
                completionSource.SetResult();
            })));

            return completionSource.Task;
        }

        public void AwaitCompletion()
        {
            TaskBatch_AwaitCompletion(ptr);
        }

        [DllImport("hyperion", EntryPoint = "TaskBatch_Create")]
        private static extern IntPtr TaskBatch_Create();

        [DllImport("hyperion", EntryPoint = "TaskBatch_Destroy")]
        private static extern void TaskBatch_Destroy(IntPtr taskBatchPtr);

        [DllImport("hyperion", EntryPoint = "TaskBatch_IsCompleted")]
        private static extern bool TaskBatch_IsCompleted(IntPtr taskBatchPtr);

        [DllImport("hyperion", EntryPoint = "TaskBatch_NumCompleted")]
        private static extern int TaskBatch_NumCompleted(IntPtr taskBatchPtr);

        [DllImport("hyperion", EntryPoint = "TaskBatch_NumEnqueued")]
        private static extern int TaskBatch_NumEnqueued(IntPtr taskBatchPtr);

        [DllImport("hyperion", EntryPoint = "TaskBatch_AddTask")]
        private static extern void TaskBatch_AddTask(IntPtr taskBatchPtr, TaskDelegate fn);

        [DllImport("hyperion", EntryPoint = "TaskBatch_AwaitCompletion")]
        private static extern void TaskBatch_AwaitCompletion(IntPtr taskBatchPtr);

        [DllImport("hyperion", EntryPoint = "TaskBatch_Launch")]
        private static extern void TaskBatch_Launch(IntPtr taskBatchPtr, IntPtr onCompletePtr);
    }
}