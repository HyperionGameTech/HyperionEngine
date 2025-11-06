using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct Property
    {
        public static readonly Property Invalid = new Property(IntPtr.Zero);

        internal IntPtr ptr;

        internal Property(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                Property_GetName(ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                Property_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        public HypData Get(ObjectBase obj)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid property");
            }

            if (!obj.IsValid)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid target object");
            }

            HypDataBuffer resultBuffer;

            if (!Property_InvokeGetter(ptr, obj.Class.Address, obj.NativeAddress, out resultBuffer))
            {
                throw new InvalidOperationException("Failed to invoke getter");
            }

            return HypData.FromBuffer(resultBuffer);
        }

        public void Set(ObjectBase obj, HypData value)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke setter: Invalid property");
            }

            if (!obj.IsValid)
            {
                throw new InvalidOperationException("Cannot invoke setter: Invalid target object");
            }

            if (value == null)
            {
                throw new ArgumentNullException("value");
            }

            if (!Property_InvokeSetter(ptr, obj.Class.Address, obj.NativeAddress, ref value.Buffer))
            {
                throw new InvalidOperationException("Failed to invoke setter");
            }
        }

        [DllImport("hyperion", EntryPoint = "Property_GetName")]
        private static extern void Property_GetName([In] IntPtr propertyPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "Property_GetTypeId")]
        private static extern void Property_GetTypeId([In] IntPtr propertyPtr, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "Property_InvokeGetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool Property_InvokeGetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [Out] out HypDataBuffer outResult);

        [DllImport("hyperion", EntryPoint = "Property_InvokeSetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool Property_InvokeSetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [In] ref HypDataBuffer value);
    }
}