using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypField
    {
        public static readonly HypField Invalid = new HypField(IntPtr.Zero);

        internal IntPtr ptr;

        internal HypField(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypField_GetName(ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                HypField_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        public uint Offset
        {
            get
            {
                return HypField_GetOffset(ptr);
            }
        }

        public object? ReadObject(object target)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new Exception("HypField pointer is null");
            }

            if (target == null)
            {
                throw new ArgumentNullException(nameof(target), "Target object cannot be null");
            }

            object? result = null;

            HypDataBuffer targetData = new HypDataBuffer();
            HypDataBuffer outData = new HypDataBuffer();

            try
            {
                targetData.SetValue(target);
                HypField_Get(ptr, ref targetData, out outData);

                result = outData.GetValue();
            }
            catch (Exception ex)
            {
                throw;
            }
            finally
            {
                targetData.Dispose();
                outData.Dispose();
            }

            return result;
        }

        [DllImport("hyperion", EntryPoint = "HypField_GetName")]
        private static extern void HypField_GetName([In] IntPtr fieldPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypField_GetTypeId")]
        private static extern void HypField_GetTypeId([In] IntPtr fieldPtr, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "HypField_GetOffset")]
        private static extern uint HypField_GetOffset([In] IntPtr fieldPtr);

        [DllImport("hyperion", EntryPoint = "HypField_Get")]
        private static extern void HypField_Get([In] IntPtr fieldPtr, [In] ref HypDataBuffer targetData, [Out] out HypDataBuffer outData);
    }
}