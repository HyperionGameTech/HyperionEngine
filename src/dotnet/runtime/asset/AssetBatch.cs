using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal delegate void HandleAssetResultsDelegate(IntPtr assetMapPtr);

    public class AssetBatch
    {
        private IntPtr ptr;

        public AssetBatch()
        {
            Console.WriteLine("Creating AssetBatch");
            this.ptr = AssetBatch_Create();
            Console.WriteLine("Created AssetBatch");
        }

        ~AssetBatch()
        {
            if (ptr == IntPtr.Zero)
            {
                throw new ObjectDisposedException("AssetBatch");
            }

            AssetBatch_Destroy(ptr);
            ptr = IntPtr.Zero;
        }

        public void Add(string key, string path)
        {
            AssetBatch_AddToBatch(ptr, key, path);
        }

        public async Task<AssetMap> Load()
        {
            var completionSource = new TaskCompletionSource<AssetMap>();
            
            AssetBatch_LoadAsync(ptr, Marshal.GetFunctionPointerForDelegate(new HandleAssetResultsDelegate((assetMapPtr) =>
            {
                if (assetMapPtr == IntPtr.Zero)
                {
                    completionSource.SetException(new Exception("Failed to load assets"));

                    return;
                }

                completionSource.SetResult(new AssetMap(assetMapPtr));
            })));

            return await completionSource.Task;
        }

        [DllImport("hyperion", EntryPoint = "AssetBatch_Create")]
        private static extern IntPtr AssetBatch_Create();

        [DllImport("hyperion", EntryPoint = "AssetBatch_Destroy")]
        private static extern void AssetBatch_Destroy(IntPtr assetBatchPtr);

        [DllImport("hyperion", EntryPoint = "AssetBatch_AddToBatch")]
        private static extern void AssetBatch_AddToBatch(IntPtr assetBatchPtr, [MarshalAs(UnmanagedType.LPStr)] string key, [MarshalAs(UnmanagedType.LPStr)] string path);

        [DllImport("hyperion", EntryPoint = "AssetBatch_LoadAsync")]
        private static extern void AssetBatch_LoadAsync(IntPtr assetBatchPtr, IntPtr handleAssetResultsPtr);

        [DllImport("hyperion", EntryPoint = "AssetBatch_AwaitResults")]
        private static extern IntPtr AssetBatch_AwaitResults(IntPtr assetBatchPtr);
    }
}
