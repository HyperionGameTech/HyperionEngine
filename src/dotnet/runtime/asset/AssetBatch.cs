using System;
using System.Runtime.InteropServices;

namespace Hyperion
{

    public class AssetBatch : IDisposable
    {
        private IntPtr ptr;

        public AssetBatch(AssetManager assetManager)
        {
            this.ptr = AssetBatch_Create(assetManager.Handle);
        }

        public void Dispose()
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

        public void LoadAsync()
        {
            AssetBatch_LoadAsync(ptr);
        }

        public AssetMap AwaitResults()
        {
            return new AssetMap(AssetBatch_AwaitResults(ptr));
        }

        [DllImport("libhyperion", EntryPoint = "AssetBatch_Create")]
        private static extern IntPtr AssetBatch_Create(IntPtr assetManagerPtr);

        [DllImport("libhyperion", EntryPoint = "AssetBatch_Destroy")]
        private static extern void AssetBatch_Destroy(IntPtr assetBatchPtr);

        [DllImport("libhyperion", EntryPoint = "AssetBatch_AddToBatch")]
        private static extern void AssetBatch_AddToBatch(IntPtr assetBatchPtr, string key, string path);

        [DllImport("libhyperion", EntryPoint = "AssetBatch_LoadAsync")]
        private static extern void AssetBatch_LoadAsync(IntPtr assetBatchPtr);

        [DllImport("libhyperion", EntryPoint = "AssetBatch_AwaitResults")]
        private static extern IntPtr AssetBatch_AwaitResults(IntPtr assetBatchPtr);
    }
}