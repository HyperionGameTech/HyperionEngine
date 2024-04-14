using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class AssetManager
    {
        private IntPtr ptr;

        public AssetManager(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public IntPtr Handle
        {
            get
            {
                return ptr;
            }
        }

        public string BasePath
        {
            get
            {
                return AssetManager_GetBasePath(ptr);
            }
            set
            {
                AssetManager_SetBasePath(ptr, value);
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetManager_GetBasePath")]
        [return: MarshalAs(UnmanagedType.LPStr)]
        private static extern string AssetManager_GetBasePath(IntPtr assetManagerPtr);

        [DllImport("hyperion", EntryPoint = "AssetManager_SetBasePath")]
        private static extern void AssetManager_SetBasePath(IntPtr assetManagerPtr, [MarshalAs(UnmanagedType.LPStr)] string basePath);
    }
}