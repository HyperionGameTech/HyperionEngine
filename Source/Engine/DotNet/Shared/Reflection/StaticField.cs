using System;
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
    }
}