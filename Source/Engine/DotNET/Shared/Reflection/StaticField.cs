using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct StaticField
    {
        public static readonly StaticField Invalid = new StaticField(IntPtr.Zero);

        internal IntPtr _ptr;

        internal StaticField(IntPtr ptr)
        {
            _ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name;
                StaticField_GetName(_ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                StaticField_GetTypeId(_ptr, out typeId);
                return typeId;
            }
        }

        public IntPtr DataPointer
        {
            get
            {
                return StaticField_GetDataPointer(_ptr);
            }
        }

        public ClassAttribute? GetAttribute(Name name)
        {
            IntPtr attributePtr = StaticField_GetAttribute(_ptr, ref name);

            if (attributePtr == IntPtr.Zero)
            {
                return null;
            }

            return new(attributePtr);
        }

        public IEnumerable<ClassAttribute> Attributes
        {
            get
            {
                uint count = StaticField_GetAttributes(_ptr, IntPtr.Zero);

                IntPtr attributesPtr = Marshal.AllocHGlobal(IntPtr.Size * (int)count);
                try
                {
                    StaticField_GetAttributes(_ptr, attributesPtr);

                    for (int i = 0; i < count; i++)
                    {
                        yield return new(Marshal.ReadIntPtr(attributesPtr, i * IntPtr.Size));
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(attributesPtr);
                }
            }
        }

        public object? ReadObject()
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new Exception("StaticField pointer is null");
            }

            object? result = null;

            BoxedValueInternal outData = new BoxedValueInternal();

            try
            {
                StaticField_Get(_ptr, out outData);

                result = outData.GetValue();
            }
            finally
            {
                outData.Dispose();
            }

            return result;
        }

        [DllImport("hyperion", EntryPoint = "StaticField_GetName")]
        private static extern void StaticField_GetName([In] IntPtr constantPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "StaticField_GetTypeId")]
        private static extern void StaticField_GetTypeId([In] IntPtr constantPtr, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "StaticField_Get")]
        private static extern bool StaticField_Get([In] IntPtr staticFieldPtr, out BoxedValueInternal outData);

        [DllImport("hyperion", EntryPoint = "StaticField_GetDataPointer")]
        private static extern IntPtr StaticField_GetDataPointer([In] IntPtr staticFieldPtr);

        [DllImport("hyperion", EntryPoint = "StaticField_GetAttribute")]
        private static extern IntPtr StaticField_GetAttribute([In] IntPtr staticFieldPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "StaticField_GetAttributes")]
        private static extern uint StaticField_GetAttributes([In] IntPtr staticFieldPtr, [Out] IntPtr attributesPtr);
    }
}