using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class AssetMap : IDisposable
    {
        private IntPtr ptr;

        public AssetMap(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public void Dispose()
        {
            if (ptr == IntPtr.Zero)
            {
                throw new ObjectDisposedException("AssetMap");
            }

            AssetMap_Destroy(ptr);
            ptr = IntPtr.Zero;
        }

        public IntPtr Handle
        {
            get
            {
                return ptr;
            }
        }

        public Asset this[string key]
        {
            get
            {
                return new Asset { ptr = AssetMap_GetAsset(ptr, key) };
            }
        }

        [DllImport("libhyperion", EntryPoint = "AssetMap_Destroy")]
        private static extern void AssetMap_Destroy(IntPtr assetMapPtr);

        [DllImport("libhyperion", EntryPoint = "AssetMap_GetAsset")]
        private static extern IntPtr AssetMap_GetAsset(IntPtr assetMapPtr, string key);
    }
}