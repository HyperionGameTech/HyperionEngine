using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct Property
    {
        public static readonly Property Invalid = new Property(IntPtr.Zero);

        internal IntPtr _ptr;

        internal Property(IntPtr ptr)
        {
            _ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name;
                Property_GetName(_ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                Property_GetTypeId(_ptr, out typeId);
                return typeId;
            }
        }

        public TypeInfo TypeInfo
        {
            get
            {
                IntPtr pTypeInfo = Property_GetTypeInfo(_ptr);
                return new TypeInfo(pTypeInfo);
            }
        }

        public IEnumerable<ClassAttribute> Attributes
        {
            get
            {
                uint count = Property_GetAttributes(_ptr, IntPtr.Zero);
                IntPtr pAttrs = Marshal.AllocHGlobal(Marshal.SizeOf<IntPtr>() * (int)count);

                try
                {
                    Property_GetAttributes(_ptr, pAttrs);

                    for (int i = 0; i < count; i++)
                    {
                        IntPtr pAttr = Marshal.ReadIntPtr(pAttrs, i * Marshal.SizeOf<IntPtr>());

                        yield return new ClassAttribute(pAttr);
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(pAttrs);
                }
            }
        }

        public ClassAttribute? GetAttribute(Name name)
        {
            IntPtr pAttr = Property_GetAttribute(_ptr, ref name);

            if (pAttr == IntPtr.Zero)
            {
                return null;
            }

            return new ClassAttribute(pAttr);
        }

        public BoxedValue Get(ObjectBase obj)
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid property");
            }

            if (!obj.IsValid)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid target object");
            }

            BoxedValueInternal resultBuffer;

            if (!Property_InvokeGetter(_ptr, obj.Class.Address, obj.NativeAddress, out resultBuffer))
            {
                throw new InvalidOperationException("Failed to invoke getter");
            }

            return BoxedValue.FromBuffer(resultBuffer);
        }

        public BoxedValue Get(IntPtr classAddress, IntPtr targetAddress)
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid property");
            }

            if (classAddress == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid class address");
            }

            if (targetAddress == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid target address");
            }

            BoxedValueInternal resultBuffer;

            if (!Property_InvokeGetter(_ptr, classAddress, targetAddress, out resultBuffer))
            {
                throw new InvalidOperationException("Failed to invoke getter");
            }

            return BoxedValue.FromBuffer(resultBuffer);
        }

        public void Set(ObjectBase obj, BoxedValue value)
        {
            if (_ptr == IntPtr.Zero)
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

            if (!Property_InvokeSetter(_ptr, obj.Class.Address, obj.NativeAddress, ref value.Buffer))
            {
                throw new InvalidOperationException("Failed to invoke setter");
            }
        }

        public void Set(IntPtr classAddress, IntPtr targetAddress, BoxedValue value)
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke setter: Invalid property");
            }

            if (classAddress == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke setter: Invalid class address");
            }

            if (targetAddress == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke setter: Invalid target address");
            }

            if (value == null)
            {
                throw new ArgumentNullException("value");
            }

            if (!Property_InvokeSetter(_ptr, classAddress, targetAddress, ref value.Buffer))
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
        private static extern bool Property_InvokeGetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [Out] out BoxedValueInternal outResult);

        [DllImport("hyperion", EntryPoint = "Property_InvokeSetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool Property_InvokeSetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [In] ref BoxedValueInternal value);

        [DllImport("hyperion", EntryPoint = "Property_GetAttributes")]
        internal static extern uint Property_GetAttributes([In] IntPtr propertyPtr, [Out] IntPtr attributesPtr);

        [DllImport("hyperion", EntryPoint = "Property_GetAttribute")]
        internal static extern IntPtr Property_GetAttribute([In] IntPtr propertyPtr, [In] ref Name name);
    }
}