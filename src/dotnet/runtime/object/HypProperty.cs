using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypProperty
    {
        public static readonly HypProperty Invalid = new HypProperty(IntPtr.Zero);

        internal IntPtr ptr;

        internal HypProperty(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypProperty_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypProperty_GetTypeID(ptr, out typeId);
                return typeId;
            }
        }
        
        public HypData InvokeGetter(HypObject hypObject)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid property");
            }

            if (!hypObject.IsValid)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid target object");
            }

            HypDataBuffer resultBuffer;

            if (!HypProperty_InvokeGetter(ptr, hypObject.HypClass.Address, hypObject.NativeAddress, out resultBuffer))
            {
                throw new InvalidOperationException("Failed to invoke getter");
            }

            return new HypData(resultBuffer);
        }

        public void InvokeSetter(HypObject hypObject, HypData value)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke setter: Invalid property");
            }

            if (!hypObject.IsValid)
            {
                throw new InvalidOperationException("Cannot invoke setter: Invalid target object");
            }

            if (value == null)
            {
                throw new ArgumentNullException("value");
            }

            if (!HypProperty_InvokeSetter(ptr, hypObject.HypClass.Address, hypObject.NativeAddress, ref value.Buffer))
            {
                throw new InvalidOperationException("Failed to invoke setter");
            }
        }
        
        [DllImport("hyperion", EntryPoint = "HypProperty_GetName")]
        private static extern void HypProperty_GetName([In] IntPtr propertyPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypProperty_GetTypeID")]
        private static extern void HypProperty_GetTypeID([In] IntPtr propertyPtr, [Out] out TypeID typeId);

        [DllImport("hyperion", EntryPoint = "HypProperty_InvokeGetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypProperty_InvokeGetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [Out] out HypDataBuffer outResult);

        [DllImport("hyperion", EntryPoint = "HypProperty_InvokeSetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypProperty_InvokeSetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [In] ref HypDataBuffer value);
    }
}