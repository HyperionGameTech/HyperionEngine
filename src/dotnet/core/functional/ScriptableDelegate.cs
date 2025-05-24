using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public class DelegateHandler : IDisposable
    {
        internal IntPtr ptr;

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

        /// <summary>
        /// Detach the delegate handle so the lifetime of the bound function is managed by the Delegate itself.
        /// </summary>
        public void Detach()
        {
            if (ptr != IntPtr.Zero)
            {
                DelegateHandler_Detach(ptr);

                Dispose();
            }
        }

        public void Remove()
        {
            if (ptr != IntPtr.Zero)
            {
                DelegateHandler_Remove(ptr);

                Dispose();
            }
        }

        [DllImport("hyperion", EntryPoint = "DelegateHandler_Detach")]
        private static extern void DelegateHandler_Detach([In] IntPtr delegateHandlerPtr);

        [DllImport("hyperion", EntryPoint = "DelegateHandler_Remove")]
        private static extern void DelegateHandler_Remove([In] IntPtr delegateHandlerPtr);

        [DllImport("hyperion", EntryPoint = "DelegateHandler_Destroy")]
        private static extern void DelegateHandler_Destroy([In] IntPtr delegateHandlerPtr);
    }

    public class DelegateWrapper
    {
        private Delegate del;

        public DelegateWrapper(Delegate del)
        {
            this.del = del;
        }

        public object? DynamicInvoke(params object[] args)
        {
            return del.DynamicInvoke(args);
        }
    }

    /// <summary>
    /// Represents a native (C++) Delegate (see core/functional/Delegate.hpp)
    /// Unrelated to C# built-in delegate type
    /// </summary>
    [NoManagedClass]
    public struct ScriptableDelegate
    {
        private object target;
        private IntPtr ptr;

        public ScriptableDelegate(object target, IntPtr ptr)
        {
            this.target = target;
            this.ptr = ptr;
        }

        public DelegateHandler Bind(Delegate del)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new Exception("Delegate is invalid");
            }

            ObjectWrapper objectWrapper = new ObjectWrapper { obj = new DelegateWrapper(del) };
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

        public void RemoveAllDetached()
        {
            if (ptr == IntPtr.Zero)
            {
                throw new Exception("Delegate is invalid");
            }

            ScriptableDelegate_RemoveAllDetached(ptr);
        }

        public bool Remove(ref DelegateHandler delegateHandler)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new Exception("Delegate is invalid");
            }

            if (delegateHandler == null)
            {
                throw new ArgumentNullException(nameof(delegateHandler));
            }

            bool wasRemoved = ScriptableDelegate_Remove(ptr, delegateHandler.ptr);

            delegateHandler.Dispose();

            return wasRemoved;
        }

        [DllImport("hyperion", EntryPoint = "ScriptableDelegate_Bind")]
        private static extern IntPtr ScriptableDelegate_Bind([In] IntPtr ptr, [In] IntPtr classObjectPtr, [In] IntPtr objectReferencePtr);

        [DllImport("hyperion", EntryPoint = "ScriptableDelegate_RemoveAllDetached")]
        private static extern int ScriptableDelegate_RemoveAllDetached([In] IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "ScriptableDelegate_Remove")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ScriptableDelegate_Remove([In] IntPtr ptr, [In] IntPtr delegateHandlerPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_AddObjectToCache")]
        private static extern void NativeInterop_AddObjectToCache([In] IntPtr objectWrapperPtr, [Out] out IntPtr outClassObjectPtr, [Out] IntPtr outObjectReferencePtr, [MarshalAs(UnmanagedType.I1)] bool isWeak);
    }
}