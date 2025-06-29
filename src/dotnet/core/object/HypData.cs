using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit)]
    internal unsafe struct InternalHypDataValue
    {
        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I1)]
        public sbyte valueI8;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I2)]
        public short valueI16;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I4)]
        public int valueI32;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I8)]
        public long valueI64;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U1)]
        public byte valueU8;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U2)]
        public ushort valueU16;
        
        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U4)]
        public uint valueU32;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U8)]
        public ulong valueU64;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.R4)]
        public float valueFloat;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.R8)]
        public double valueDouble;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I1)]
        public bool valueBool;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U8)]
        public ulong valueId;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U8)]
        public Name valueName;

        // pointer type
        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I8)]
        public IntPtr valueIntPtr;
        
        [FieldOffset(0)]
        public ObjectReference objectReference;
    }

    /// <summary>
    ///  Represents HypData.hpp from the core/object library
    ///  Needs to be a struct to be passed by value, has a fixed size of 32 bytes
    ///  Destructor needs to be called manually (HypData_Destruct)
    /// </summary>
    [StructLayout(LayoutKind.Explicit, Size = 40, Pack = 8)]
    public unsafe struct HypDataBuffer : IDisposable
    {
        [FieldOffset(0)]
        private fixed byte buffer[32];

        [FieldOffset(32)]
        private IntPtr serializeFunctionPtr;

        public void Dispose()
        {
            HypData_Destruct(ref this);
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                HypData_GetTypeId(ref this, out typeId);
                return typeId;
            }
        }

        public IntPtr Pointer
        {
            get
            {
                return HypData_GetPointer(ref this);
            }
        }

        public bool IsNull
        {
            get
            {
                return HypData_IsNull(ref this);
            }
        }

        public void SetValue(object? value)
        {
            if (value == null)
            {
                HypData_Reset(ref this);
                return;
            }

            if (value is sbyte)
            {
                HypData_SetInt8(ref this, (sbyte)value);
                return;
            }

            if (value is short)
            {
                HypData_SetInt16(ref this, (short)value);
                return;
            }

            if (value is int)
            {
                HypData_SetInt32(ref this, (int)value);
                return;
            }

            if (value is long)
            {
                HypData_SetInt64(ref this, (long)value);
                return;
            }

            if (value is byte)
            {
                HypData_SetUInt8(ref this, (byte)value);
                return;
            }

            if (value is ushort)
            {
                HypData_SetUInt16(ref this, (ushort)value);
                return;
            }

            if (value is uint)
            {
                HypData_SetUInt32(ref this, (uint)value);
                return;
            }

            if (value is ulong)
            {
                HypData_SetUInt64(ref this, (ulong)value);
                return;
            }

            if (value is float)
            {
                HypData_SetFloat(ref this, (float)value);
                return;
            }

            if (value is double)
            {
                HypData_SetDouble(ref this, (double)value);
                return;
            }

            if (value is bool)
            {
                HypData_SetBool(ref this, (bool)value);
                return;
            }

            if (value is IntPtr)
            {
                HypData_SetIntPtr(ref this, (IntPtr)value);
                return;
            }

            if (value is ObjIdBase valueId)
            {
                HypData_SetId(ref this, ref valueId);
                return;
            }

            if (value is Name)
            {
                HypData_SetName(ref this, (Name)value);
                return;
            }

            if (value is HypObject)
            {
                HypObject obj = (HypObject)value;

                if (!obj.HypClass.IsReferenceCounted)
                    throw new Exception("Cannot use HypData_SetHypObject with non reference counted HypClass type from managed code");

                if (!HypData_SetHypObject(ref this, obj.HypClass.Address, obj.NativeAddress))
                {
                    throw new InvalidOperationException("Failed to set HypData to HypObject instance for HypClass: " + obj.HypClass.Name);
                }

                return;
            }

            if (value is string)
            {
                string str = (string)value;

                // Set Utf8 string
                IntPtr stringPtr = Marshal.StringToCoTaskMemUTF8(str);

                if (!HypData_SetString(ref this, stringPtr))
                {
                    Marshal.FreeCoTaskMem(stringPtr);
                    
                    throw new InvalidOperationException("Failed to set string");
                }

                Marshal.FreeCoTaskMem(stringPtr);

                return;
            }

            if (value is byte[])
            {
                unsafe
                {
                    byte[] buffer = (byte[])value;

                    fixed (byte* ptr = buffer)
                    {
                        if (!HypData_SetByteBuffer(ref this, (IntPtr)ptr, (uint)buffer.Length))
                        {
                            throw new InvalidOperationException("Failed to set byte buffer");
                        }
                    }
                }

                return;
            }

            Type type = value.GetType();

            if (type.IsValueType)
            {
                if (type.IsEnum)
                {
                    type = Enum.GetUnderlyingType(type);

                    if (type == typeof(sbyte))
                    {
                        HypData_SetInt8(ref this, (sbyte)value);
                        return;
                    }

                    if (type == typeof(short))
                    {
                        HypData_SetInt16(ref this, (short)value);
                        return;
                    }

                    if (type == typeof(int))
                    {
                        HypData_SetInt32(ref this, (int)value);
                        return;
                    }

                    if (type == typeof(long))
                    {
                        HypData_SetInt64(ref this, (long)value);
                        return;
                    }

                    if (type == typeof(byte))
                    {
                        HypData_SetUInt8(ref this, (byte)value);
                        return;
                    }

                    if (type == typeof(ushort))
                    {
                        HypData_SetUInt16(ref this, (ushort)value);
                        return;
                    }

                    if (type == typeof(uint))
                    {
                        HypData_SetUInt32(ref this, (uint)value);
                        return;
                    }

                    if (type == typeof(ulong))
                    {
                        HypData_SetUInt64(ref this, (ulong)value);
                        return;
                    }

                    throw new NotImplementedException("Unsupported enum type to construct HypData: " + type.FullName);
                }

                HypClass? hypClass = null;

                if (HypStructHelpers.IsHypStruct(value.GetType(), out hypClass))
                {
                    unsafe
                    {
                        Span<byte> buffer = stackalloc byte[Marshal.SizeOf(value)];

                        fixed (byte* ptr = buffer)
                        {
                            Marshal.StructureToPtr(value, (IntPtr)ptr, false);

                            if (!HypData_SetHypStruct(ref this, ((HypClass)hypClass).Address, (uint)Marshal.SizeOf(value), (IntPtr)ptr))
                            {
                                throw new InvalidOperationException("Failed to set HypStruct");
                            }
                        }

                        return;
                    }
                }
            }
            else if (type.IsArray)
            {
                Array array = (Array)value;

                HypClass? hypClass = HypClass.TryGetClass(type.GetElementType());

                if (hypClass == null)
                {
                    throw new InvalidOperationException("Failed to get HypClass for type: " + type.GetElementType()?.FullName);
                }

                unsafe
                {
                    // Create array of HypData
                    HypDataBuffer[] hypDataBufferArray = new HypDataBuffer[array.Length];

                    for (int i = 0; i < hypDataBufferArray.Length; i++)
                    {
                        hypDataBufferArray[i].SetValue(array.GetValue(i));
                    }

                    try
                    {
                        fixed (HypDataBuffer* ptr = hypDataBufferArray)
                        {
                            if (!HypData_SetArray(ref this, ((HypClass)hypClass).Address, (IntPtr)ptr, (uint)hypDataBufferArray.Length))
                            {
                                throw new InvalidOperationException("Failed to set array!");
                            }

                            Logger.Log(LogType.Debug, "HypData.SetValue: Set array of type " + ((HypClass)hypClass).Name + " with length " + hypDataBufferArray.Length + " type Id: " + this.TypeId.Value);
                        }
                    }
                    finally
                    {
                        // Dispose all HypDataBuffer instances
                        foreach (HypDataBuffer hypDataBuffer in hypDataBufferArray)
                        {
                            hypDataBuffer.Dispose();
                        }
                    }
                }

                return;
            }

            throw new NotImplementedException("Unsupported type to construct HypData: " + type.FullName);
        }

        public unsafe object? GetValue()
        {
            if (IsNull)
            {
                return null;
            }

            InternalHypDataValue value;

            if (HypData_GetInt8(ref this, true, out value.valueI8))
            {
                return value.valueI8;
            }

            if (HypData_GetInt16(ref this, true, out value.valueI16))
            {
                return value.valueI16;
            }

            if (HypData_GetInt32(ref this, true, out value.valueI32))
            {
                return value.valueI32;
            }

            if (HypData_GetInt64(ref this, true, out value.valueI64))
            {
                return value.valueI64;
            }

            if (HypData_GetUInt8(ref this, true, out value.valueU8))
            {
                return value.valueU8;
            }

            if (HypData_GetUInt16(ref this, true, out value.valueU16))
            {
                return value.valueU16;
            }

            if (HypData_GetUInt32(ref this, true, out value.valueU32))
            {
                return value.valueU32;
            }

            if (HypData_GetUInt64(ref this, true, out value.valueU64))
            {
                return value.valueU64;
            }

            if (HypData_GetFloat(ref this, true, out value.valueFloat))
            {
                return value.valueFloat;
            }

            if (HypData_GetDouble(ref this, true, out value.valueDouble))
            {
                return value.valueDouble;
            }

            if (HypData_GetBool(ref this, true, out value.valueBool))
            {
                return value.valueBool;
            }

            if (HypData_GetIntPtr(ref this, true, out value.valueIntPtr))
            {
                return value.valueIntPtr;
            }

            if (HypData_GetId(ref this, out value.valueId))
            {
                return new ObjIdBase(new TypeId((uint)(value.valueId >> 32)), (uint)(value.valueId & 0xFFFFFFFFu));
            }

            if (HypData_GetName(ref this, out value.valueName))
            {
                return value.valueName;
            }

            if (HypData_IsString(ref this))
            {
                IntPtr stringPtr;

                if (!HypData_GetString(ref this, out stringPtr))
                {
                    throw new InvalidOperationException("Failed to get string");
                }

                return Marshal.PtrToStringUTF8(stringPtr);
            }

            // if (HypData_IsArray(ref this))
            // {
            //     IntPtr arrayPtr;
            //     uint arraySize;

            //     if (!HypData_GetArray(ref this, out arrayPtr, out arraySize))
            //     {
            //         throw new InvalidOperationException("Failed to get array");
            //     }

            //     object[] array = new object[arraySize];

            //     unsafe
            //     {
            //         HypDataBuffer* ptr = (HypDataBuffer*)arrayPtr.ToPointer();

            //         for (int i = 0; i < arraySize; i++)
            //         {
            //             array[i] = ptr[i].GetValue();
            //         }
            //     }

            //     return array;
            // }

            if (HypData_IsByteBuffer(ref this))
            {
                IntPtr bufferPtr;
                uint bufferSize;

                if (!HypData_GetByteBuffer(ref this, out bufferPtr, out bufferSize))
                {
                    throw new InvalidOperationException("Failed to get byte buffer");
                }

                byte[] buffer = new byte[bufferSize];

                unsafe
                {
                    byte* ptr = (byte*)bufferPtr.ToPointer();

                    for (int i = 0; i < bufferSize; i++)
                    {
                        buffer[i] = ptr[i];
                    }
                }

                return buffer;
            }

            if (HypData_GetHypObject(ref this, out value.objectReference))
            {
                return value.objectReference.LoadObject();
            }

            if (HypData_GetHypStruct(ref this, out value.objectReference))
            {
                return value.objectReference.LoadObject();
            }

            if (DynamicHypStruct.TryGet(TypeId, out DynamicHypStruct? dynamicHypStruct))
            {
                return dynamicHypStruct.MarshalFromHypData(ref this);
            }

            throw new NotImplementedException("Unsupported type to get value from HypData. Current TypeId: " + TypeId.Value);
        }

        public sbyte ReadInt8()
        {
            if (IsNull)
            {
                return 0;
            }

            sbyte value;

            if (HypData_GetInt8(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get sbyte from HypData");
        }

        public short ReadInt16()
        {
            if (IsNull)
            {
                return 0;
            }

            short value;

            if (HypData_GetInt16(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get short from HypData");
        }

        public int ReadInt32()
        {
            if (IsNull)
            {
                return 0;
            }

            int value;

            if (HypData_GetInt32(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get int from HypData");
        }

        public long ReadInt64()
        {
            if (IsNull)
            {
                return 0;
            }

            long value;

            if (HypData_GetInt64(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get long from HypData");
        }

        public byte ReadUInt8()
        {
            if (IsNull)
            {
                return 0;
            }

            byte value;

            if (HypData_GetUInt8(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get byte from HypData");
        }

        public ushort ReadUInt16()
        {
            if (IsNull)
            {
                return 0;
            }

            ushort value;

            if (HypData_GetUInt16(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get ushort from HypData");
        }

        public uint ReadUInt32()
        {
            if (IsNull)
            {
                return 0;
            }

            uint value;

            if (HypData_GetUInt32(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get uint from HypData");
        }

        public ulong ReadUInt64()
        {
            if (IsNull)
            {
                return 0;
            }

            ulong value;

            if (HypData_GetUInt64(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get ulong from HypData");
        }

        public float ReadFloat()
        {
            if (IsNull)
            {
                return 0f;
            }

            float value;

            if (HypData_GetFloat(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get float from HypData");
        }

        public double ReadDouble()
        {
            if (IsNull)
            {
                return 0.0;
            }

            double value;

            if (HypData_GetDouble(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get double from HypData");
        }

        public bool ReadBool()
        {
            if (IsNull)
            {
                return false;
            }

            bool value;

            if (HypData_GetBool(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get bool from HypData");
        }

        public IntPtr ReadIntPtr()
        {
            if (IsNull)
            {
                return IntPtr.Zero;
            }

            InternalHypDataValue value;

            if (HypData_GetIntPtr(ref this, true, out value.valueIntPtr))
            {
                return value.valueIntPtr;
            }

            throw new InvalidOperationException("Failed to get IntPtr from HypData");
        }

        public string ReadString()
        {
            if (IsNull)
            {
                return string.Empty;
            }

            IntPtr stringPtr;

            if (HypData_GetString(ref this, out stringPtr))
            {
                return Marshal.PtrToStringUTF8(stringPtr) ?? string.Empty;
            }

            throw new InvalidOperationException("Failed to get string from HypData");
        }

        public Name ReadName()
        {
            if (IsNull)
            {
                return Name.Invalid;
            }

            Name name;

            if (HypData_GetName(ref this, out name))
            {
                return name;
            }

            throw new InvalidOperationException("Failed to get Name from HypData");
        }

        public ObjIdBase ReadId()
        {
            if (IsNull)
            {
                return ObjIdBase.Invalid;
            }

            ulong idValue;

            if (HypData_GetId(ref this, out idValue))
            {
                return new ObjIdBase(new TypeId((uint)(idValue >> 32)), (uint)(idValue & 0xFFFFFFFFu));
            }

            throw new InvalidOperationException("Failed to get Id from HypData");
        }

        public T? ReadObject<T>() where T : HypObject
        {
            if (IsNull)
            {
                return default(T);
            }

            ObjectReference objectReference;

            if (HypData_GetHypObject(ref this, out objectReference))
            {
                return (T?)objectReference.LoadObject();
            }

            throw new InvalidOperationException("Failed to get HypObject from HypData");
        }

        public T ReadStruct<T>() where T : struct
        {
            if (IsNull)
            {
                throw new InvalidOperationException("Cannot read struct from null HypData");
            }

            ObjectReference objectReference;

            if (HypData_GetHypStruct(ref this, out objectReference))
            {
                return (T)objectReference.LoadObject();
            }

            if (DynamicHypStruct.TryGet(TypeId, out DynamicHypStruct? dynamicHypStruct))
            {
                return (T)dynamicHypStruct.MarshalFromHypData(ref this);
            }

            throw new NotImplementedException("Unsupported type to get struct from HypData. Current TypeId: " + TypeId.Value);
        }

        public byte[] ReadByteBuffer()
        {
            if (IsNull)
            {
                return Array.Empty<byte>();
            }

            IntPtr bufferPtr;
            uint bufferSize;

            if (!HypData_GetByteBuffer(ref this, out bufferPtr, out bufferSize))
            {
                throw new InvalidOperationException("Failed to get byte buffer");
            }

            byte[] buffer = new byte[bufferSize];

            unsafe
            {
                void* src = bufferPtr.ToPointer();

                // Memcopy the buffer
                fixed (byte* dest = buffer)
                {
                    Buffer.MemoryCopy(src, dest, bufferSize, bufferSize);
                }
            }

            return buffer;
        }

        [DllImport("hyperion", EntryPoint = "HypData_Construct")]
        internal static extern void HypData_Construct([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_Destruct")]
        internal static extern void HypData_Destruct([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_Reset")]
        internal static extern void HypData_Reset([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_GetTypeId")]
        internal static extern void HypData_GetTypeId([In] ref HypDataBuffer hypData, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "HypData_GetPointer")]
        internal static extern IntPtr HypData_GetPointer([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsNull")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsNull([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt8([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out sbyte outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt16([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out short outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt32([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out int outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt64([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out long outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt8([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out byte outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt16([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out ushort outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt32([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out uint outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt64([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out ulong outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetFloat([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out float outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetDouble([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out double outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetBool([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out bool outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetIntPtr")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetIntPtr([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out IntPtr outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetArray")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetArray([In] ref HypDataBuffer hypData, [Out] out IntPtr outArrayPtr, [Out] out uint outArraySize);

        [DllImport("hyperion", EntryPoint = "HypData_GetString")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetString([In] ref HypDataBuffer hypData, [Out] out IntPtr outStringPtr);

        [DllImport("hyperion", EntryPoint = "HypData_GetId")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetId([In] ref HypDataBuffer hypData, [Out] out ulong outIdValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetName")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetName([In] ref HypDataBuffer hypData, [Out] out Name outNameValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetHypObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetHypObject([In] ref HypDataBuffer hypData, [Out] out ObjectReference outObjectReference);

        [DllImport("hyperion", EntryPoint = "HypData_GetHypStruct")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetHypStruct([In] ref HypDataBuffer hypData, [Out] out ObjectReference outObjectReference);

        [DllImport("hyperion", EntryPoint = "HypData_GetByteBuffer")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetByteBuffer([In] ref HypDataBuffer hypData, [Out] out IntPtr outBufferPtr, [Out] out uint outBufferSize);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt8([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt16([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt32([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt64([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt8([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt16([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt32([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt64([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsFloat([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsDouble([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsBool([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsIntPtr")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsIntPtr([In] ref HypDataBuffer hypData, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "HypData_IsArray")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsArray([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsString")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsString([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsByteBuffer")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsByteBuffer([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsId")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsId([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsName")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsName([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt8([In] ref HypDataBuffer hypData, sbyte value);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt16([In] ref HypDataBuffer hypData, short value);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt32([In] ref HypDataBuffer hypData, int value);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt64([In] ref HypDataBuffer hypData, long value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt8([In] ref HypDataBuffer hypData, byte value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt16([In] ref HypDataBuffer hypData, ushort value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt32([In] ref HypDataBuffer hypData, uint value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt64([In] ref HypDataBuffer hypData, ulong value);

        [DllImport("hyperion", EntryPoint = "HypData_SetFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetFloat([In] ref HypDataBuffer hypData, float value);

        [DllImport("hyperion", EntryPoint = "HypData_SetDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetDouble([In] ref HypDataBuffer hypData, double value);

        [DllImport("hyperion", EntryPoint = "HypData_SetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetBool([In] ref HypDataBuffer hypData, bool value);

        [DllImport("hyperion", EntryPoint = "HypData_SetIntPtr")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetIntPtr([In] ref HypDataBuffer hypData, IntPtr value);

        [DllImport("hyperion", EntryPoint = "HypData_SetArray")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetArray([In] ref HypDataBuffer hypData, [In] IntPtr hypClassPtr, [In] IntPtr arrayPtr, uint arraySize);

        [DllImport("hyperion", EntryPoint = "HypData_SetString")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetString([In] ref HypDataBuffer hypData, [In] IntPtr stringPtr);

        [DllImport("hyperion", EntryPoint = "HypData_SetId")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetId([In] ref HypDataBuffer hypData, ref ObjIdBase id);

        [DllImport("hyperion", EntryPoint = "HypData_SetName")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetName([In] ref HypDataBuffer hypData, Name name);

        [DllImport("hyperion", EntryPoint = "HypData_SetHypObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetHypObject([In] ref HypDataBuffer hypData, [In] IntPtr hypClassPtr, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "HypData_SetHypStruct")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetHypStruct([In] ref HypDataBuffer hypData, [In] IntPtr hypClassPtr, uint objectSize, [In] IntPtr objectPtr);

        [DllImport("hyperion", EntryPoint = "HypData_SetByteBuffer")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetByteBuffer([In] ref HypDataBuffer hypData, [In] IntPtr bufferPtr, uint bufferSize);
    }

    public class HypData : IDisposable
    {
        private HypDataBuffer _data;
        private bool _disposed = false;

        private HypData()
        {
        }

        public HypData(object? value)
        {
            HypDataBuffer.HypData_Construct(ref _data);

            if (value != null)
            {
                _data.SetValue(value);
            }
        }

        public static HypData FromBuffer(HypDataBuffer data)
        {
            HypData hypData = new HypData();
            hypData._data = data;

            return hypData;
        }
        
        public void Dispose()
        {
            if (!_disposed)
            {
                _data.Dispose();
                _disposed = true;
            }
        }

        ~HypData()
        {
            Dispose();
        }

        public TypeId TypeId
        {
            get
            {
                return _data.TypeId;
            }
        }

        public IntPtr Pointer
        {
            get
            {
                return _data.Pointer;
            }
        }

        public bool IsNull
        {
            get
            {
                return _data.IsNull;
            }
        }

        public ref HypDataBuffer Buffer
        {
            get
            {
                return ref _data;
            }
        }

        public object? GetValue()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(HypData));
            }

            return _data.GetValue();
        }

        public void SetValue(object? value)
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(HypData));
            }

            _data.SetValue(value);
        }

        public override string ToString()
        {
            if (_disposed)
            {
                return "Disposed";
            }

            return GetValue()?.ToString() ?? "null";
        }
    }
}