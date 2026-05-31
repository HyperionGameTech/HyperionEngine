using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct Field
    {
        public static readonly Field Invalid = new Field(IntPtr.Zero);

        internal IntPtr ptr;

        internal Field(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                Field_GetName(ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                Field_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        public uint Offset
        {
            get
            {
                return Field_GetOffset(ptr);
            }
        }

        public object? ReadObject(object target)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new Exception("Field pointer is null");
            }

            if (target == null)
            {
                throw new ArgumentNullException(nameof(target), "Target object cannot be null");
            }

            object? result = null;

            BoxedValueInternal targetData = new BoxedValueInternal();
            BoxedValueInternal outData = new BoxedValueInternal();

            try
            {
                targetData.SetValue(target);
                Field_Get(ptr, ref targetData, out outData);

                result = outData.GetValue();
            }
            finally
            {
                targetData.Dispose();
                outData.Dispose();
            }

            return result;
        }

        [DllImport("hyperion", EntryPoint = "Field_GetName")]
        private static extern void Field_GetName([In] IntPtr fieldPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "Field_GetTypeId")]
        private static extern void Field_GetTypeId([In] IntPtr fieldPtr, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "Field_GetOffset")]
        private static extern uint Field_GetOffset([In] IntPtr fieldPtr);

        [DllImport("hyperion", EntryPoint = "Field_Get")]
        private static extern void Field_Get([In] IntPtr fieldPtr, [In] ref BoxedValueInternal targetData, [Out] out BoxedValueInternal outData);
    }
}