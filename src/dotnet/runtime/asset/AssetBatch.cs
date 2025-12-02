using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal delegate void HandleAssetResultsDelegate(IntPtr assetMapPtr);

    public class AssetBatch : IDisposable
    {
        private IntPtr m_ptr;
        private bool m_wasLoadEnqueued;

        public AssetBatch()
        {
            m_wasLoadEnqueued = false;
            m_ptr = AssetBatch_Create();
        }

        ~AssetBatch()
        {
            if (m_wasLoadEnqueued)
            {
                return; // if load was enqueued, memory will be released by asset manager
            }

            if (m_ptr == IntPtr.Zero)
            {
                throw new ObjectDisposedException("AssetBatch");
            }

            AssetBatch_Destroy(m_ptr);
            m_ptr = IntPtr.Zero;
        }

        public void Dispose()
        {
            if (m_wasLoadEnqueued)
            {
                return; // if load was enqueued, memory will be released by asset manager
            }

            if (m_ptr != IntPtr.Zero)
            {
                AssetBatch_Destroy(m_ptr);
                m_ptr = IntPtr.Zero;
            }

            GC.SuppressFinalize(this);
        }

        public void Add(string key, string path)
        {
            if (m_wasLoadEnqueued)
            {
                throw new InvalidOperationException("Cannot add assets after Load has been called on this AssetBatch");
            }

            AssetBatch_AddToBatch(m_ptr, key, path);
        }

        public Task<AssetMap> Load()
        {
            if (m_wasLoadEnqueued)
            {
                throw new InvalidOperationException("Load has already been called on this AssetBatch");
            }

            GCHandle? gcHandle = null;

            var completionSource = new TaskCompletionSource<AssetMap>();

            var del = new HandleAssetResultsDelegate((pAssetMap) =>
            {
                if (pAssetMap == IntPtr.Zero)
                {
                    completionSource.SetException(new Exception("Failed to load assets"));

                    gcHandle?.Free();

                    return;
                }

                completionSource.SetResult(new AssetMap(pAssetMap));

                gcHandle?.Free();
            }); 
            
            gcHandle = GCHandle.Alloc(del);

            AssetBatch_LoadAsync(m_ptr, Marshal.GetFunctionPointerForDelegate(del));

            m_wasLoadEnqueued = true;

            return completionSource.Task;
        }

        [DllImport("hyperion", EntryPoint = "AssetBatch_Create")]
        private static extern IntPtr AssetBatch_Create();

        [DllImport("hyperion", EntryPoint = "AssetBatch_Destroy")]
        private static extern void AssetBatch_Destroy(IntPtr pBatch);

        [DllImport("hyperion", EntryPoint = "AssetBatch_AddToBatch")]
        private static extern void AssetBatch_AddToBatch(IntPtr pBatch, [MarshalAs(UnmanagedType.LPStr)] string key, [MarshalAs(UnmanagedType.LPStr)] string path);

        [DllImport("hyperion", EntryPoint = "AssetBatch_LoadAsync")]
        private static extern void AssetBatch_LoadAsync(IntPtr pBatch, IntPtr pFnCallback);

        [DllImport("hyperion", EntryPoint = "AssetBatch_AwaitResults")]
        private static extern IntPtr AssetBatch_AwaitResults(IntPtr pBatch);
    }
}
