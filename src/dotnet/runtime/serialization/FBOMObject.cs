using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class FBOMObject : IDisposable
    {

        internal IntPtr ptr;

        public FBOMObject()
        {
            this.ptr = FBOMObject_Create();
        }

        internal FBOMObject(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        ~FBOMObject()
        {
            if (ptr != IntPtr.Zero)
            {
                FBOMObject_Destroy(ptr);
            }
        }

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                FBOMObject_Destroy(ptr);
                ptr = IntPtr.Zero;
            }

            GC.SuppressFinalize(this);
        }

        public FBOMData? this[string key]
        {
            get
            {
                FBOMData result = new FBOMData();

                if (!FBOMObject_GetProperty(ptr, key, result.ptr))
                {
                    return null;
                }

                return result;
            }
            set
            {
                if (value == null)
                {
                    throw new ArgumentNullException("value");
                }

                FBOMObject_SetProperty(ptr, key, value.ptr);
            }
        }

        // public T GetDeserializedValue<T>()
        // {
        //     IntPtr deserializedValuePtr = FBOMObject_GetDeserializedValue(ptr);

        //     if (deserializedValuePtr == IntPtr.Zero)
        //     {
        //         return default(T);
        //     }

        //     object obj;
        //     MarshalHelpers.MarshalInObject(deserializedValuePtr, typeof(T), out obj);

        //     return (T)obj;
        // }

        [DllImport("hyperion", EntryPoint = "FBOMObject_Create")]
        private static extern IntPtr FBOMObject_Create();

        [DllImport("hyperion", EntryPoint = "FBOMObject_Destroy")]
        private static extern void FBOMObject_Destroy([In] IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "FBOMObject_GetProperty")]
        private static extern bool FBOMObject_GetProperty([In] IntPtr ptr, [MarshalAs(UnmanagedType.LPStr)] string key, [Out] IntPtr outDataPtr);

        [DllImport("hyperion", EntryPoint = "FBOMObject_SetProperty")]
        private static extern bool FBOMObject_SetProperty([In] IntPtr ptr, [MarshalAs(UnmanagedType.LPStr)] string key, [In] IntPtr dataPtr);

        // [DllImport("hyperion", EntryPoint = "FBOMObject_GetDeserializedValue")]
        // private static extern IntPtr FBOMObject_GetDeserializedValue([In] IntPtr ptr);
    }
}