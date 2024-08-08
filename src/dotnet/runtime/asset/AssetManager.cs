using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding]
    public class AssetManager : HypObject
    {
        public static AssetManager Instance
        {
            get
            {
                return new AssetManager(AssetManager_GetInstance());
            }
        }

        private IntPtr ptr;

        public AssetManager()
        {
        }

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
                IntPtr stringPtr = AssetManager_GetBasePath(ptr);
                
                return Marshal.PtrToStringAnsi(stringPtr);
            }
            set
            {
                AssetManager_SetBasePath(ptr, value);
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetManager_GetInstance")]
        private static extern IntPtr AssetManager_GetInstance();

        [DllImport("hyperion", EntryPoint = "AssetManager_GetBasePath")]
        private static extern IntPtr AssetManager_GetBasePath(IntPtr assetManagerPtr);

        [DllImport("hyperion", EntryPoint = "AssetManager_SetBasePath")]
        private static extern void AssetManager_SetBasePath(IntPtr assetManagerPtr, [MarshalAs(UnmanagedType.LPStr)] string basePath);
    }
}