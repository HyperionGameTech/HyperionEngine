using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class SimThread
    {
        public static async Task<T> PostTask<T>(Func<T> func)
        {
            TaskCompletionSource<T> tcs = new TaskCompletionSource<T>(TaskCreationOptions.RunContinuationsAsynchronously);

            GCHandle gcHandle = default;

            Action? inner = () =>
            {
                try
                {
                    T result = func.Invoke();
                    tcs.SetResult(result);
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
                finally
                {
                    gcHandle.Free();
                }
            };

            gcHandle = GCHandle.Alloc(inner);

            IntPtr pAction = Marshal.GetFunctionPointerForDelegate(inner);
            SimThread_PostTask(pAction);

            /// From: https://learn.microsoft.com/en-us/dotnet/api/system.threading.tasks.task.configureawait
            /// When an asynchronous method awaits a Task directly,
            /// continuation usually occurs in the same thread that created the task,
            /// depending on the async context.
            /// This behavior can be costly in terms of performance and can result
            /// in a deadlock on the UI thread
            /// To avoid these problems, call Task.ConfigureAwait(false)
            return await tcs.Task.ConfigureAwait(false);
        }

        public static async Task PostTask(Action action)
        {
            TaskCompletionSource tcs = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

            GCHandle gcHandle = default;

            Action? inner = () =>
            {
                if (gcHandle == null)
                {
                    throw new Exception("GCHandle is null in callback");
                }

                try
                {
                    action.Invoke();
                    tcs.SetResult();
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
                finally
                {
                    gcHandle.Free();
                }
            };

            gcHandle = GCHandle.Alloc(inner);

            IntPtr pAction = Marshal.GetFunctionPointerForDelegate(inner);
            SimThread_PostTask(pAction);

            await tcs.Task.ConfigureAwait(false);
        }

        [DllImport("hyperion", EntryPoint = "SimThread_PostTask")]
        private static extern void SimThread_PostTask(IntPtr pAction);
    }
}