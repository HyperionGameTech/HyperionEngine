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
                return Marshal.PtrToStringAnsi(AssetManager_GetBasePath(ptr));
            }
            set
            {

                IntPtr valuePtr = Marshal.StringToHGlobalAnsi(value);

                AssetManager_SetBasePath(ptr, valuePtr);

                Marshal.FreeHGlobal(valuePtr);
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetManager_GetBasePath")]
        private static extern IntPtr AssetManager_GetBasePath(IntPtr assetManagerPtr);

        [DllImport("hyperion", EntryPoint = "AssetManager_SetBasePath")]
        private static extern void AssetManager_SetBasePath(IntPtr assetManagerPtr, IntPtr basePathPtr);
    }
}