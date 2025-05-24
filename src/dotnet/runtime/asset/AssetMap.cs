using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class AssetMap
    {
        private IntPtr ptr;

        public AssetMap(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        ~AssetMap()
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
                return new Asset(AssetMap_GetAsset(ptr, key));
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetMap_Destroy")]
        private static extern void AssetMap_Destroy(IntPtr assetMapPtr);

        [DllImport("hyperion", EntryPoint = "AssetMap_GetAsset")]
        private static extern IntPtr AssetMap_GetAsset(IntPtr assetMapPtr, [MarshalAs(UnmanagedType.LPStr)] string keyPtr);
    }
}