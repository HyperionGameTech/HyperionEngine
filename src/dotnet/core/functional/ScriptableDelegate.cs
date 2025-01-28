using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public class DelegateHandler : IDisposable
    {
        private IntPtr ptr;

        internal DelegateHandler(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        ~DelegateHandler()
        {
            if (ptr != IntPtr.Zero)
            {
                DelegateHandler_Destroy(ptr);
            }
        }

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                DelegateHandler_Destroy(ptr);
                ptr = IntPtr.Zero;
            }

            GC.SuppressFinalize(this);
        }

        [DllImport("hyperion", EntryPoint = "DelegateHandler_Destroy")]
        private static extern void DelegateHandler_Destroy([In] IntPtr delegateHandlerPtr);
    }

    /// <summary>
    /// Represents a native (C++) Delegate (see core/functional/Delegate.hpp)
    /// Unrelated to C# built-in delegate type
    /// </summary>
    public struct ScriptableDelegate
    {
        private IntPtr ptr;

        public ScriptableDelegate(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public DelegateHandler Bind(object delegateObject)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new Exception("Delegate is invalid");
            }

            ObjectWrapper objectWrapper = new ObjectWrapper { obj = delegateObject };
            ObjectReference objectReference = new ObjectReference();

            unsafe
            {
                IntPtr objectWrapperPtr = (IntPtr)Unsafe.AsPointer(ref objectWrapper);
                IntPtr objectReferencePtr = (IntPtr)Unsafe.AsPointer(ref objectReference);

                IntPtr classObjectPtr = IntPtr.Zero;

                NativeInterop_AddObjectToCache(objectWrapperPtr, out classObjectPtr, objectReferencePtr, isWeak: true);

#if DEBUG
                if (!objectReference.IsValid)
                {
                    throw new Exception("Failed to add object to cache");
                }
#endif

                IntPtr delegateHandlerPtr = ScriptableDelegate_Bind(ptr, classObjectPtr, objectReferencePtr);

                return new DelegateHandler(delegateHandlerPtr);
            }
        }

        [DllImport("hyperion", EntryPoint = "ScriptableDelegate_Bind")]
        private static extern IntPtr ScriptableDelegate_Bind([In] IntPtr ptr, [In] IntPtr classObjectPtr, [In] IntPtr objectReferencePtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_AddObjectToCache")]
        private static extern void NativeInterop_AddObjectToCache([In] IntPtr objectWrapperPtr, [Out] out IntPtr outClassObjectPtr, [Out] IntPtr outObjectReferencePtr, [MarshalAs(UnmanagedType.I1)] bool isWeak);
    }
}