using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class AssetMap
    {
        public AssetMap(IntPtr ptr)
        {
            Handle = ptr;
        }

        ~AssetMap()
        {
            if (Handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException("AssetMap");
            }

            AssetMap_Destroy(Handle);
            Handle = IntPtr.Zero;
        }

        public IntPtr Handle { get; private set; }

        public LoadedAsset this[string key]
        {
            get
            {
                return new LoadedAsset(AssetMap_GetAsset(Handle, key));
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetMap_Destroy")]
        private static extern void AssetMap_Destroy(IntPtr assetMapPtr);

        [DllImport("hyperion", EntryPoint = "AssetMap_GetAsset")]
        private static extern IntPtr AssetMap_GetAsset(IntPtr assetMapPtr, [MarshalAs(UnmanagedType.LPStr)] string keyPtr);
    }
}