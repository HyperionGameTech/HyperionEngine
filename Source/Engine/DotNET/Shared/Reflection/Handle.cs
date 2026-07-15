using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static partial class ManagedHandleNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "Object_GetId")]
        internal static extern void Object_GetId([In] IntPtr nativeAddress, [Out] out ObjIdBase outIdValue);

        [DllImport("hyperion", EntryPoint = "Handle_Get")]
        internal static extern void Handle_Get(IntPtr ptr, [Out] out BoxedValueInternal outBoxed);

        [DllImport("hyperion", EntryPoint = "Handle_Set")]
        internal static extern void Handle_Set([In] ref BoxedValueInternal boxedInternal, [Out] out IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "Handle_Destruct")]
        internal static extern void Handle_Destruct(IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "WeakHandle_Lock")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static extern bool WeakHandle_Lock(IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "WeakHandle_Set")]
        internal static extern void WeakHandle_Set([In] ref BoxedValueInternal boxedInternal, [Out] out IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "WeakHandle_Destruct")]
        internal static extern void WeakHandle_Destruct(IntPtr ptr);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct Handle : IDisposable
    {
        internal IntPtr ptr;

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                ManagedHandleNativeBindings.Handle_Destruct(ptr);
                ptr = IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public object? GetValue()
        {
            if (ptr == IntPtr.Zero)
            {
                return null;
            }

            BoxedValueInternal boxedInternal;
            ManagedHandleNativeBindings.Handle_Get(ptr, out boxedInternal);

            object? value = boxedInternal.GetValue();

            boxedInternal.Dispose();

            return value;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct WeakHandle : IDisposable
    {
        internal IntPtr ptr;

        public WeakHandle()
        {
            this.ptr = IntPtr.Zero;
        }

        public WeakHandle(object? value)
        {
            if (value == null)
            {
                ptr = IntPtr.Zero;
                return;
            }

            using (BoxedValue boxed = new BoxedValue(value))
            {
                ManagedHandleNativeBindings.WeakHandle_Set(ref boxed.Buffer, out ptr);
            }
        }

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                ManagedHandleNativeBindings.WeakHandle_Destruct(ptr);
                ptr = IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Handle Lock()
        {
            if (ptr == IntPtr.Zero)
            {
                return new Handle();
            }

            if (ManagedHandleNativeBindings.WeakHandle_Lock(ptr))
            {
                Handle handle = new Handle();
                handle.ptr = ptr;
                return handle;
            }
            else
            {
                return new Handle();
            }
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct Handle<T> : IDisposable where T : ObjectBase
    {
        public static readonly Handle<T> Empty = new Handle<T>();

        internal IntPtr ptr;

        public Handle()
        {
            this.ptr = IntPtr.Zero;
        }

        public Handle(T? value)
        {
            if (value == null)
            {
                ptr = IntPtr.Zero;
                return;
            }

            Class? cls = Class.GetClass<T>();

            if (cls == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered Class");
            }

            BoxedValueInternal boxedInternal = new BoxedValueInternal();
            boxedInternal.SetValue((T)value);

            ManagedHandleNativeBindings.Handle_Set(ref boxedInternal, out ptr);

            boxedInternal.Dispose();
        }

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                ManagedHandleNativeBindings.Handle_Destruct(ptr);
                ptr = IntPtr.Zero;
            }
        }

        public ObjIdBase Id
        {
            get
            {
                ObjIdBase id;
                ManagedHandleNativeBindings.Object_GetId(ptr, out id);
                return id;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public T? GetValue()
        {
            if (ptr == IntPtr.Zero)
            {
                return null;
            }

            BoxedValueInternal boxedInternal;
            ManagedHandleNativeBindings.Handle_Get(ptr, out boxedInternal);

            T? value = (T?)boxedInternal.GetValue();

            boxedInternal.Dispose();

            return value;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct WeakHandle<T> : IDisposable where T : ObjectBase
    {
        public static readonly WeakHandle<T> Empty = new WeakHandle<T>();

        internal IntPtr ptr;

        public WeakHandle()
        {
            this.ptr = IntPtr.Zero;
        }

        public WeakHandle(T? value)
        {
            if (value == null)
            {
                ptr = IntPtr.Zero;

                return;
            }

            using (BoxedValue boxed = new BoxedValue(value))
            {
                ManagedHandleNativeBindings.WeakHandle_Set(ref boxed.Buffer, out ptr);
            }
        }

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                ManagedHandleNativeBindings.WeakHandle_Destruct(ptr);
                ptr = IntPtr.Zero;
            }
        }

        public ObjIdBase Id
        {
            get
            {
                ObjIdBase id;
                ManagedHandleNativeBindings.Object_GetId(ptr, out id);
                return id;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Handle<T> Lock()
        {
            if (ptr == IntPtr.Zero)
            {
                return Handle<T>.Empty;
            }

            if (ManagedHandleNativeBindings.WeakHandle_Lock(ptr))
            {
                Handle<T> handle = new Handle<T>();
                handle.ptr = ptr;
                return handle;
            }
            else
            {
                return Handle<T>.Empty;
            }
        }
    }
}