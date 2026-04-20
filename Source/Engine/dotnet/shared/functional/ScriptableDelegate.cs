using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public class DelegateHandler : IDisposable
    {
        internal IntPtr _ptr;
        // Keeps the managed closure (DelegateWrapper) alive for as long as this handler
        // is alive. Without this, the GC can collect the closure between Bind() and the
        // first invocation, causing the weak GCHandle in ObjectReference to return null.
        internal object? _closure;

        internal DelegateHandler(IntPtr ptr)
        {
            _ptr = ptr;
        }

        ~DelegateHandler()
        {
            if (_ptr != IntPtr.Zero)
            {
                DelegateHandler_Destroy(_ptr);
            }
        }

        public void Dispose()
        {
            if (_ptr != IntPtr.Zero)
            {
                DelegateHandler_Destroy(_ptr);
                _ptr = IntPtr.Zero;
            }

            _closure = null;

            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Detach the delegate handle so the lifetime of the bound function is managed by the Delegate itself.
        /// </summary>
        public void Detach()
        {
            if (_ptr != IntPtr.Zero)
            {
                DelegateHandler_Detach(_ptr);
                DelegateHandler_Destroy(_ptr);
                _ptr = IntPtr.Zero;
            }

            _closure = null;
        }

        public void Remove()
        {
            if (_ptr != IntPtr.Zero)
            {
                DelegateHandler_Remove(_ptr);
                DelegateHandler_Destroy(_ptr);
                _ptr = IntPtr.Zero;
            }

            _closure = null;
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
    [NoNativeClass]
    public struct ScriptableDelegate
    {
        private object _target;
        private IntPtr _ptr;

        public ScriptableDelegate(object target, IntPtr ptr)
        {
            _target = target;
            _ptr = ptr;
        }

        public DelegateHandler Bind(Delegate del)
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new Exception("Delegate is invalid");
            }

            ObjectWrapper objectWrapper = new ObjectWrapper { obj = new DelegateWrapper(del) };
            ObjectReference objectReference = new ObjectReference();

            unsafe
            {
                IntPtr objectWrapperPtr = (IntPtr)Unsafe.AsPointer(ref objectWrapper);
                IntPtr objectReferencePtr = (IntPtr)Unsafe.AsPointer(ref objectReference);

                IntPtr pClass = IntPtr.Zero;

                NativeInterop_AddObjectToCache(objectWrapperPtr, out pClass, objectReferencePtr, isWeak: true);

#if DEBUG
                if (!objectReference.IsValid)
                {
                    throw new Exception("Failed to add object to cache");
                }
#endif

                IntPtr delegateHandlerPtr = ScriptableDelegate_Bind(_ptr, pClass, objectReferencePtr);

                return new DelegateHandler(delegateHandlerPtr)
                {
                    _closure = objectWrapper.obj  // strong ref keeps DelegateWrapper from being GC'd
                };
            }
        }

        public void RemoveAllDetached()
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new Exception("Delegate is invalid");
            }

            ScriptableDelegate_RemoveAllDetached(_ptr);
        }

        public bool Remove(ref DelegateHandler delegateHandler)
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new Exception("Delegate is invalid");
            }

            if (delegateHandler == null)
            {
                throw new ArgumentNullException(nameof(delegateHandler));
            }

            bool wasRemoved = ScriptableDelegate_Remove(_ptr, delegateHandler._ptr);

            delegateHandler.Dispose();

            return wasRemoved;
        }

        [DllImport("hyperion", EntryPoint = "ScriptableDelegate_Bind")]
        private static extern IntPtr ScriptableDelegate_Bind([In] IntPtr ptr, [In] IntPtr pClass, [In] IntPtr objectReferencePtr);

        [DllImport("hyperion", EntryPoint = "ScriptableDelegate_RemoveAllDetached")]
        private static extern int ScriptableDelegate_RemoveAllDetached([In] IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "ScriptableDelegate_Remove")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ScriptableDelegate_Remove([In] IntPtr ptr, [In] IntPtr delegateHandlerPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_AddObjectToCache")]
        private static extern void NativeInterop_AddObjectToCache([In] IntPtr objectWrapperPtr, [Out] out IntPtr outClassObjectPtr, [Out] IntPtr outObjectReferencePtr, [MarshalAs(UnmanagedType.I1)] bool isWeak);
    }
}