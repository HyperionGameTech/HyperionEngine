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

        public TypeInfo TypeInfo
        {
            get
            {
                IntPtr typeInfoPtr = Property_GetTypeInfo(ptr);
                return new TypeInfo(typeInfoPtr);
            }
        }

        public IEnumerable<ClassAttribute> Attributes
        {
            get
            {
                uint count = Property_GetAttributes(ptr, IntPtr.Zero);
                IntPtr attributesPtr = Marshal.AllocHGlobal(Marshal.SizeOf<IntPtr>() * (int)count);

                try
                {
                    Property_GetAttributes(ptr, attributesPtr);

                    for (int i = 0; i < count; i++)
                    {
                        IntPtr attributePtr = Marshal.ReadIntPtr(attributesPtr, i * Marshal.SizeOf<IntPtr>());

                        yield return new ClassAttribute(attributePtr);
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(attributesPtr);
                }
            }
        }

        public ClassAttribute? GetAttribute(Name name)
        {
            IntPtr attributePtr = Property_GetAttribute(ptr, ref name);

            if (attributePtr == IntPtr.Zero)
            {
                return null;
            }

            return new ClassAttribute(attributePtr);
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

        [DllImport("hyperion", EntryPoint = "Property_GetTypeInfo")]
        internal static extern IntPtr Property_GetTypeInfo([In] IntPtr propertyPtr);

        [DllImport("hyperion", EntryPoint = "Property_InvokeGetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool Property_InvokeGetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [Out] out HypDataBuffer outResult);

        [DllImport("hyperion", EntryPoint = "Property_InvokeSetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool Property_InvokeSetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [In] ref HypDataBuffer value);

        [DllImport("hyperion", EntryPoint = "Property_GetAttributes")]
        internal static extern uint Property_GetAttributes([In] IntPtr propertyPtr, [Out] IntPtr attributesPtr);

        [DllImport("hyperion", EntryPoint = "Property_GetAttribute")]
        internal static extern IntPtr Property_GetAttribute([In] IntPtr propertyPtr, [In] ref Name name);
    }
}