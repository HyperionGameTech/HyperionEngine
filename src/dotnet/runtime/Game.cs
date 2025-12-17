using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "Game")]
    public abstract class Game : ObjectBase
    {
        public World World
        {
            get => this.GetWorld();
        }

        public abstract void OnLaunch();
        public abstract void OnUpdate(float deltaTime);

        public async Task<T> PostTask<T>(Func<T> func)
        {
            TaskCompletionSource<T> tcs = new TaskCompletionSource<T>(TaskCreationOptions.RunContinuationsAsynchronously);

            GCHandle gcHandle = default;

            Action? inner = () =>
            {
                if (gcHandle == null)
                {
                    throw new Exception("GCHandle is null in callback");
                }

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
            Game_PostTask(NativeAddress, pAction);

            /// From: https://learn.microsoft.com/en-us/dotnet/api/system.threading.tasks.task.configureawait
            /// When an asynchronous method awaits a Task directly,
            /// continuation usually occurs in the same thread that created the task,
            /// depending on the async context.
            /// This behavior can be costly in terms of performance and can result
            /// in a deadlock on the UI thread
            /// To avoid these problems, call Task.ConfigureAwait(false)
            return await tcs.Task.ConfigureAwait(false);
        }

        public async Task PostTask(Action action)
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
            Game_PostTask(NativeAddress, pAction);

            await tcs.Task.ConfigureAwait(false);
        }

        [DllImport("hyperion", EntryPoint = "Game_PostTask")]
        private static extern void Game_PostTask(IntPtr pGame, IntPtr pAction);
    }
}