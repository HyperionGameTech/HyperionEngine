using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public class MarshalHelpers
    {
        public static unsafe void MarshalInObject(IntPtr ptr, Type type, out object obj)
        {
            // Helper for strings
            if (type == typeof(string))
            {
                obj = Marshal.PtrToStringAnsi(Marshal.ReadIntPtr(ptr));

                return;
            }

            if (type == typeof(IntPtr))
            {
                obj = Marshal.ReadIntPtr(ptr);

                return;
            }

            if (type.IsClass)
            {
                object* objectPtr = (object*)Marshal.ReadIntPtr(ptr).ToPointer();

                if (objectPtr == null)
                {
                    throw new NullReferenceException("Marshaled object pointer is null!");
                }

                obj = *objectPtr;

                return;
            }

            if (type.IsValueType)
            {
                obj = Marshal.PtrToStructure(ptr, type);

                return;
            }

            if (type.IsPointer)
            {
                throw new NotImplementedException("Pointer marshal in not implemented");
            }

            if (type.IsByRef)
            {
                throw new NotImplementedException("ByRef type marshal in not implemented");
            }

            throw new NotImplementedException("Type not implemented for marshaling in");
        }

        public static unsafe void MarshalOutObject(IntPtr outPtr, Type type, object? obj)
        {
            // If returnType is an enum we need to get the underlying type and cast the value to it
            if (type.IsEnum)
            {
                type = Enum.GetUnderlyingType(type);
                obj = Convert.ChangeType(obj, type);
            }

            if (type == typeof(void))
            {
                // No need to fill out the outPtr
                return;
            }

            if (type == typeof(string))
            {
                throw new NotImplementedException("String return type not implemented");
            }

            if (type == typeof(bool))
            {
                Marshal.WriteByte(outPtr, (byte)((bool)obj ? 1 : 0));

                return;
            }

            // write the return value to the outPtr
            // there MUST be enough space allocated at the outPtr,
            // or else this will cause memory corruption
            if (type.IsValueType)
            {
                Marshal.StructureToPtr(obj, outPtr, false);

                return;
            }

            if (type == typeof(IntPtr))
            {
                Marshal.WriteIntPtr(outPtr, (IntPtr)obj);

                return;
            }

            throw new NotImplementedException("Return type not implemented");
        }
    }
}
