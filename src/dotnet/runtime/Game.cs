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
            TaskCompletionSource<T> tcs = new TaskCompletionSource<T>();

            GCHandle? gcHandle = null;

            Action? inner = () =>
            {
                if (gcHandle == null)
                {
                    throw new Exception("GCHandle is null in callback");
                }

                try
                {
                    tcs.SetResult(func.Invoke());
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
                finally
                {
                    gcHandle.Value.Free();
                }
            };

            gcHandle = GCHandle.Alloc(inner);

            IntPtr pAction = Marshal.GetFunctionPointerForDelegate(inner);
            Game_PostTask(NativeAddress, pAction);

            return await tcs.Task;
        }

        public async Task PostTask(Action action)
        {
            TaskCompletionSource tcs = new TaskCompletionSource();

            GCHandle? gcHandle = null;

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
                    gcHandle.Value.Free();
                }
            };

            gcHandle = GCHandle.Alloc(inner);

            IntPtr pAction = Marshal.GetFunctionPointerForDelegate(inner);
            Game_PostTask(NativeAddress, pAction);

            await tcs.Task;
        }

       [DllImport("hyperion", EntryPoint = "Game_PostTask")]
        private static extern void Game_PostTask(IntPtr pGame, IntPtr pAction);
    }
}