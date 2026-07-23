using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Diagnostics;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit)]
    internal unsafe struct BoxedValueUnion
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
    ///  Represents BoxedValue.hpp from the core/object library
    ///  Needs to be a struct to be passed by value, has a fixed size of 32 bytes
    ///  Destructor needs to be called manually (BoxedValue_Destruct)
    /// </summary>
    [StructLayout(LayoutKind.Explicit, Size = 32, Pack = 8)]
    public unsafe struct BoxedValueInternal : IDisposable
    {
        [FieldOffset(0)]
        private fixed byte _buffer[24];

        [FieldOffset(24)]
        private IntPtr _serializeFunc;

        public void Dispose()
        {
            BoxedValue_Destruct(ref this);
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                BoxedValue_GetTypeId(ref this, out typeId);
                return typeId;
            }
        }

        public TypeInfo TypeInfo
        {
            get
            {
                IntPtr pTypeInfo = BoxedValue_GetTypeInfo(ref this);
                return new TypeInfo(pTypeInfo);
            }
        }

        public IntPtr Pointer
        {
            get
            {
                return BoxedValue_GetPointer(ref this);
            }
        }

        public bool IsNull
        {
            get
            {
                return BoxedValue_IsNull(ref this);
            }
        }

        public void SetValue(object? value)
        {
            if (value == null)
            {
                BoxedValue_SetNullObject(ref this);
                return;
            }

            if (value is sbyte _i8)
            {
                BoxedValue_SetInt8(ref this, _i8);
                return;
            }

            if (value is short _i16)
            {
                BoxedValue_SetInt16(ref this, _i16);
                return;
            }

            if (value is int _i32)
            {
                BoxedValue_SetInt32(ref this, _i32);
                return;
            }

            if (value is long _i64)
            {
                BoxedValue_SetInt64(ref this, _i64);
                return;
            }

            if (value is byte _u8)
            {
                BoxedValue_SetUInt8(ref this, _u8);
                return;
            }

            if (value is ushort _u16)
            {
                BoxedValue_SetUInt16(ref this, _u16);
                return;
            }

            if (value is uint _u32)
            {
                BoxedValue_SetUInt32(ref this, _u32);
                return;
            }

            if (value is ulong _u64)
            {
                BoxedValue_SetUInt64(ref this, _u64);
                return;
            }

            if (value is float _f32)
            {
                BoxedValue_SetFloat(ref this, _f32);
                return;
            }

            if (value is double _f64)
            {
                BoxedValue_SetDouble(ref this, _f64);
                return;
            }

            if (value is bool _bool)
            {
                BoxedValue_SetBool(ref this, _bool);
                return;
            }

            if (value is IntPtr _nint)
            {
                BoxedValue_SetIntPtr(ref this, _nint);
                return;
            }

            if (value is ObjIdBase _objId)
            {
                BoxedValue_SetId(ref this, ref _objId);
                return;
            }

            if (value is Name _name)
            {
                BoxedValue_SetName(ref this, _name);
                return;
            }

            if (value is ObjectBase _obj)
            {
                if (!BoxedValue_SetObject(ref this, _obj.Class.Address, _obj.NativeAddress))
                {
                    throw new InvalidOperationException("Failed to set BoxedValue to ObjectBase instance for Class: " + _obj.Class.Name);
                }

                return;
            }

            if (value is string _str)
            {
                IntPtr pString = IntPtr.Zero;

                try
                {
                    // Set Utf8 string
                    pString = Marshal.StringToCoTaskMemUTF8(_str);

                    if (!BoxedValue_SetString(ref this, pString))
                    {
                        throw new InvalidOperationException("Failed to set string");
                    }
                }
                finally
                {
                    if (pString != IntPtr.Zero)
                    {
                        Marshal.FreeCoTaskMem(pString);
                    }
                }

                return;
            }

            if (value is byte[] _bytes)
            {
                unsafe
                {
                    fixed (byte* pBytes = _bytes)
                    {
                        if (!BoxedValue_SetByteBuffer(ref this, (IntPtr)pBytes, (uint)_bytes.Length))
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

                    switch (Type.GetTypeCode(type))
                    {
                    case TypeCode.SByte:
                        BoxedValue_SetInt8(ref this, (sbyte)value);
                        return;
                    case TypeCode.Int16:
                        BoxedValue_SetInt16(ref this, (short)value);
                        return;
                    case TypeCode.Int32:
                        BoxedValue_SetInt32(ref this, (int)value);
                        return;
                    case TypeCode.Int64:
                        BoxedValue_SetInt64(ref this, (long)value);
                        return;
                    case TypeCode.Byte:
                        BoxedValue_SetUInt8(ref this, (byte)value);
                        return;
                    case TypeCode.UInt16:
                        BoxedValue_SetUInt16(ref this, (ushort)value);
                        return;
                    case TypeCode.UInt32:
                        BoxedValue_SetUInt32(ref this, (uint)value);
                        return;
                    case TypeCode.UInt64:
                        BoxedValue_SetUInt64(ref this, (ulong)value);
                        return;
                    }

                    throw new NotImplementedException("Unsupported enum type to construct BoxedValue: " + type.FullName);
                }

                Class? cls = null;

                if (StructHelpers.IsStruct(value.GetType(), out cls))
                {
                    unsafe
                    {
                        Span<byte> buffer = stackalloc byte[Marshal.SizeOf(value)];

                        fixed (byte* pBuffer = buffer)
                        {
                            Marshal.StructureToPtr(value, (IntPtr)pBuffer, false);

                            if (!BoxedValue_SetStruct(ref this, ((Class)cls).Address, (uint)Marshal.SizeOf(value), (IntPtr)pBuffer))
                            {
                                throw new InvalidOperationException("Failed to set Struct");
                            }
                        }

                        return;
                    }
                }
            }
            else if (type.IsArray)
            {
                Array array = (Array)value;
                Type elementType = type.GetElementType() ?? throw new("Not an array");

                Class? cls = Class.TryGetClass(elementType);

                if (cls == null)
                {
                    throw new InvalidOperationException("Failed to get Class for type: " + elementType.FullName);
                }

                unsafe
                {
                    // Create array of BoxedValue
                    BoxedValueInternal[] arr = new BoxedValueInternal[array.Length];

                    for (int i = 0; i < arr.Length; i++)
                    {
                        arr[i].SetValue(array.GetValue(i));
                    }

                    try
                    {
                        fixed (BoxedValueInternal* ptr = arr)
                        {
                            if (!BoxedValue_SetArray(ref this, ((Class)cls).Address, (IntPtr)ptr, (uint)arr.Length))
                            {
                                throw new InvalidOperationException("Failed to set array!");
                            }
                        }
                    }
                    finally
                    {
                        // Dispose all BoxedValueInternal instances
                        foreach (BoxedValueInternal boxedInternal in arr)
                        {
                            boxedInternal.Dispose();
                        }
                    }
                }

                return;
            }

#if DEBUG
            string? hierarchyStr = null;

            try
            {
                List<string> classHierarchy = new List<string>();
                Type? currentType = type;

                while (currentType != null)
                {
                    //// attempting to figure out why some types with ObjectBase aren't being picked up
                    //// (Mismatched assemblies?)
                    //if (currentType.Name.Contains("ObjectBase"))
                    //{
                    //    Logger.Log(LogLevel.Debug, "Type is derived from ObjectBase but failed to set BoxedValue. Type: " + type.FullName + ", ref'd ObjectBase Assembly path: " + currentType.Assembly.Location + ", our ObjectBase path: " + typeof(ObjectBase).Assembly.Location
                    //        + ", is assignable from ObjectBase: " + typeof(ObjectBase).IsAssignableFrom(currentType)
                    //        + ", equal types : " + (typeof(ObjectBase) == currentType));
                    //}

                    classHierarchy.Add(currentType.Name + ", Assembly path: " + currentType.Assembly.Location);
                    currentType = currentType.BaseType;
                }

                hierarchyStr = string.Join(" -> ", classHierarchy.ToArray());
            }
            catch
            {
            }

            if (hierarchyStr != null)
            {
                throw new NotImplementedException($"Unsupported type to set BoxedValue: {type.FullName}, Assembly path: {type.Assembly.Location}, hierarchy: {hierarchyStr}");
            }
#endif

            throw new NotImplementedException($"Unsupported type to set BoxedValue: {type.FullName}, Assembly path: {type.Assembly.Location}");
        }

        public unsafe object? GetValue()
        {
            if (IsNull)
            {
                return null;
            }

            BoxedValueUnion value;

            if (BoxedValue_GetInt8(ref this, true, out value.valueI8))
            {
                return value.valueI8;
            }

            if (BoxedValue_GetInt16(ref this, true, out value.valueI16))
            {
                return value.valueI16;
            }

            if (BoxedValue_GetInt32(ref this, true, out value.valueI32))
            {
                return value.valueI32;
            }

            if (BoxedValue_GetInt64(ref this, true, out value.valueI64))
            {
                return value.valueI64;
            }

            if (BoxedValue_GetUInt8(ref this, true, out value.valueU8))
            {
                return value.valueU8;
            }

            if (BoxedValue_GetUInt16(ref this, true, out value.valueU16))
            {
                return value.valueU16;
            }

            if (BoxedValue_GetUInt32(ref this, true, out value.valueU32))
            {
                return value.valueU32;
            }

            if (BoxedValue_GetUInt64(ref this, true, out value.valueU64))
            {
                return value.valueU64;
            }

            if (BoxedValue_GetFloat(ref this, true, out value.valueFloat))
            {
                return value.valueFloat;
            }

            if (BoxedValue_GetDouble(ref this, true, out value.valueDouble))
            {
                return value.valueDouble;
            }

            if (BoxedValue_GetBool(ref this, true, out value.valueBool))
            {
                return value.valueBool;
            }

            if (BoxedValue_GetIntPtr(ref this, true, out value.valueIntPtr))
            {
                return value.valueIntPtr;
            }

            if (BoxedValue_GetId(ref this, out value.valueId))
            {
                return new ObjIdBase(new TypeId((uint)(value.valueId >> 32)), (uint)(value.valueId & 0xFFFFFFFFu));
            }

            if (BoxedValue_GetName(ref this, out value.valueName))
            {
                return value.valueName;
            }

            if (BoxedValue_IsString(ref this))
            {
                IntPtr stringPtr;

                if (!BoxedValue_GetString(ref this, out stringPtr))
                {
                    throw new InvalidOperationException("Failed to get string");
                }

                return Marshal.PtrToStringUTF8(stringPtr);
            }

            if (BoxedValue_IsArray(ref this))
            {
                int arraySize;

                if (!BoxedValue_GetArraySize(ref this, out arraySize))
                {
                    throw new InvalidOperationException("Failed to get array size");
                }

                object?[] array = new object?[arraySize];

                for (int i = 0; i < arraySize; i++)
                {
                    BoxedValueInternal elem;
                    if (!BoxedValue_GetArrayElem(ref this, i, &elem))
                    {
                        throw new InvalidOperationException("Failed to get array element at index " + i);
                    }

                    array[i] = elem.GetValue() ?? null;
                    elem.Dispose();
                }

                return array;
            }

            if (BoxedValue_IsByteBuffer(ref this))
            {
                IntPtr bufferPtr;
                uint bufferSize;

                if (!BoxedValue_GetByteBuffer(ref this, out bufferPtr, out bufferSize))
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

            if (BoxedValue_GetObject(ref this, out value.objectReference))
            {
                return value.objectReference.LoadObject();
            }

            if (BoxedValue_GetStruct(ref this, out value.objectReference))
            {
                return value.objectReference.LoadObject();
            }

            if (DynamicStruct.TryGet(TypeId, out DynamicStruct? dynamicStruct))
            {
                return dynamicStruct.MarshalFromBoxed(ref this);
            }

            throw new NotImplementedException("Unsupported type to get value from BoxedValue. Current TypeId: " + TypeId.Value);
        }

        public sbyte ReadInt8()
        {
            if (IsNull)
            {
                return 0;
            }

            sbyte value;

            if (BoxedValue_GetInt8(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get sbyte from BoxedValue");
        }

        public short ReadInt16()
        {
            if (IsNull)
            {
                return 0;
            }

            short value;

            if (BoxedValue_GetInt16(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get short from BoxedValue");
        }

        public int ReadInt32()
        {
            if (IsNull)
            {
                return 0;
            }

            int value;

            if (BoxedValue_GetInt32(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get int from BoxedValue");
        }

        public long ReadInt64()
        {
            if (IsNull)
            {
                return 0;
            }

            long value;

            if (BoxedValue_GetInt64(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get long from BoxedValue");
        }

        public byte ReadUInt8()
        {
            if (IsNull)
            {
                return 0;
            }

            byte value;

            if (BoxedValue_GetUInt8(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get byte from BoxedValue");
        }

        public ushort ReadUInt16()
        {
            if (IsNull)
            {
                return 0;
            }

            ushort value;

            if (BoxedValue_GetUInt16(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get ushort from BoxedValue");
        }

        public uint ReadUInt32()
        {
            if (IsNull)
            {
                return 0;
            }

            uint value;

            if (BoxedValue_GetUInt32(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get uint from BoxedValue");
        }

        public ulong ReadUInt64()
        {
            if (IsNull)
            {
                return 0;
            }

            ulong value;

            if (BoxedValue_GetUInt64(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get ulong from BoxedValue");
        }

        public float ReadFloat()
        {
            if (IsNull)
            {
                return 0f;
            }

            float value;

            if (BoxedValue_GetFloat(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get float from BoxedValue");
        }

        public double ReadDouble()
        {
            if (IsNull)
            {
                return 0.0;
            }

            double value;

            if (BoxedValue_GetDouble(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get double from BoxedValue");
        }

        public bool ReadBool()
        {
            if (IsNull)
            {
                return false;
            }

            bool value;

            if (BoxedValue_GetBool(ref this, false, out value))
            {
                return value;
            }

            throw new InvalidOperationException("Failed to get bool from BoxedValue");
        }

        public IntPtr ReadIntPtr()
        {
            if (IsNull)
            {
                return IntPtr.Zero;
            }

            BoxedValueUnion value;

            if (BoxedValue_GetIntPtr(ref this, true, out value.valueIntPtr))
            {
                return value.valueIntPtr;
            }

            throw new InvalidOperationException("Failed to get IntPtr from BoxedValue");
        }

        public string ReadString()
        {
            if (IsNull)
            {
                return string.Empty;
            }

            IntPtr stringPtr;

            if (BoxedValue_GetString(ref this, out stringPtr))
            {
                return Marshal.PtrToStringUTF8(stringPtr) ?? string.Empty;
            }

            throw new InvalidOperationException("Failed to get string from BoxedValue");
        }

        public Name ReadName()
        {
            if (IsNull)
            {
                return Name.Invalid;
            }

            Name name;

            if (BoxedValue_GetName(ref this, out name))
            {
                return name;
            }

            throw new InvalidOperationException("Failed to get Name from BoxedValue");
        }

        public ObjIdBase ReadId()
        {
            if (IsNull)
            {
                return ObjIdBase.Invalid;
            }

            ulong idValue;

            if (BoxedValue_GetId(ref this, out idValue))
            {
                return new ObjIdBase(new TypeId((uint)(idValue >> 32)), (uint)(idValue & 0xFFFFFFFFu));
            }

            throw new InvalidOperationException("Failed to get Id from BoxedValue");
        }

        public T? ReadObject<T>() where T : ObjectBase
        {
            if (IsNull)
            {
                return default(T);
            }

            ObjectReference objectReference;

            if (BoxedValue_GetObject(ref this, out objectReference))
            {
                return (T?)objectReference.LoadObject();
            }

            throw new InvalidOperationException("Failed to get ObjectBase from BoxedValue");
        }

        public T ReadStruct<T>() where T : struct
        {
            if (IsNull)
            {
                throw new InvalidOperationException("Cannot read struct from null BoxedValue");
            }

            ObjectReference objectReference;

            if (BoxedValue_GetStruct(ref this, out objectReference))
            {
                return (T)objectReference.LoadObject();
            }

            if (DynamicStruct.TryGet(TypeId, out DynamicStruct? dynamicStruct))
            {
                return (T)dynamicStruct.MarshalFromBoxed(ref this);
            }

            throw new NotImplementedException("Unsupported type to get struct from BoxedValue. Current TypeId: " + TypeId.Value);
        }

        public byte[] ReadByteBuffer()
        {
            if (IsNull)
            {
                return Array.Empty<byte>();
            }

            IntPtr bufferPtr;
            uint bufferSize;

            if (!BoxedValue_GetByteBuffer(ref this, out bufferPtr, out bufferSize))
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

        [DllImport("hyperion", EntryPoint = "BoxedValue_Construct")]
        internal static extern void BoxedValue_Construct([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_Destruct")]
        internal static extern void BoxedValue_Destruct([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_Reset")]
        internal static extern void BoxedValue_Reset([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetTypeId")]
        internal static extern void BoxedValue_GetTypeId([In] ref BoxedValueInternal boxed, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetTypeInfo")]
        internal static extern IntPtr BoxedValue_GetTypeInfo([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetPointer")]
        internal static extern IntPtr BoxedValue_GetPointer([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsNull")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsNull([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetInt8([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out sbyte outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetInt16([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out short outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetInt32([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out int outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetInt64([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out long outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetUInt8([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out byte outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetUInt16([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out ushort outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetUInt32([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out uint outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetUInt64([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out ulong outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetFloat([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out float outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetDouble([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out double outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetBool([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out bool outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetIntPtr")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetIntPtr([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict, [Out] out IntPtr outValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetArraySize")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetArraySize([In] ref BoxedValueInternal boxed, [Out] out int outSize);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetArrayElem")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetArrayElem([In] ref BoxedValueInternal boxed, int index, [Out] BoxedValueInternal* outArrayElem);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetString")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetString([In] ref BoxedValueInternal boxed, [Out] out IntPtr outStringPtr);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetId")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetId([In] ref BoxedValueInternal boxed, [Out] out ulong outIdValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetName")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetName([In] ref BoxedValueInternal boxed, [Out] out Name outNameValue);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetObject([In] ref BoxedValueInternal boxed, [Out] out ObjectReference outObjectReference);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetStruct")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetStruct([In] ref BoxedValueInternal boxed, [Out] out ObjectReference outObjectReference);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetByteBuffer")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_GetByteBuffer([In] ref BoxedValueInternal boxed, [Out] out IntPtr outBufferPtr, [Out] out uint outBufferSize);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsInt8([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsInt16([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsInt32([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsInt64([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsUInt8([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsUInt16([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsUInt32([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsUInt64([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsFloat([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsDouble([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsBool([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsIntPtr")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsIntPtr([In] ref BoxedValueInternal boxed, [MarshalAs(UnmanagedType.I1)] bool strict);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsArray")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsArray([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsString")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsString([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsByteBuffer")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsByteBuffer([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsId")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsId([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_IsName")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_IsName([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetInt8([In] ref BoxedValueInternal boxed, sbyte value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetInt16([In] ref BoxedValueInternal boxed, short value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetInt32([In] ref BoxedValueInternal boxed, int value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetInt64([In] ref BoxedValueInternal boxed, long value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetUInt8([In] ref BoxedValueInternal boxed, byte value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetUInt16([In] ref BoxedValueInternal boxed, ushort value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetUInt32([In] ref BoxedValueInternal boxed, uint value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetUInt64([In] ref BoxedValueInternal boxed, ulong value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetFloat([In] ref BoxedValueInternal boxed, float value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetDouble([In] ref BoxedValueInternal boxed, double value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetBool([In] ref BoxedValueInternal boxed, bool value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetIntPtr")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetIntPtr([In] ref BoxedValueInternal boxed, IntPtr value);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetArray")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetArray([In] ref BoxedValueInternal boxed, [In] IntPtr pClass, [In] IntPtr arrayPtr, uint arraySize);

        [DllImport("hyperion", EntryPoint = "BoxedValue_GetArrayElemTypeInfo")]
        internal static extern IntPtr BoxedValue_GetArrayElemTypeInfo([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetArrayElem")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetArrayElem([In] ref BoxedValueInternal boxed, int index, [In] ref BoxedValueInternal elem);

        [DllImport("hyperion", EntryPoint = "BoxedValue_PushBackArrayElem")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_PushBackArrayElem([In] ref BoxedValueInternal boxed, [In] ref BoxedValueInternal elem);

        [DllImport("hyperion", EntryPoint = "BoxedValue_ResizeArray")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_ResizeArray([In] ref BoxedValueInternal boxed, int newSize);

        [DllImport("hyperion", EntryPoint = "BoxedValue_RemoveArrayElement")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_RemoveArrayElement([In] ref BoxedValueInternal boxed, int index);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetString")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetString([In] ref BoxedValueInternal boxed, [In] IntPtr stringPtr);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetId")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetId([In] ref BoxedValueInternal boxed, ref ObjIdBase id);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetName")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetName([In] ref BoxedValueInternal boxed, Name name);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetObject([In] ref BoxedValueInternal boxed, [In] IntPtr pClass, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetNullObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetNullObject([In] ref BoxedValueInternal boxed);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetStruct")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetStruct([In] ref BoxedValueInternal boxed, [In] IntPtr pClass, uint objectSize, [In] IntPtr pStructData);

        [DllImport("hyperion", EntryPoint = "BoxedValue_SetByteBuffer")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool BoxedValue_SetByteBuffer([In] ref BoxedValueInternal boxed, [In] IntPtr bufferPtr, uint bufferSize);
    }

    public class BoxedValue : IDisposable
    {
        private BoxedValueInternal _data;
        private bool _disposed = false;

        private BoxedValue()
        {
        }

        public BoxedValue(object? value)
        {
            BoxedValueInternal.BoxedValue_Construct(ref _data);
            _data.SetValue(value);
        }

        public static BoxedValue FromBuffer(BoxedValueInternal data)
        {
            BoxedValue boxed = new BoxedValue();
            boxed._data = data;

            return boxed;
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _data.Dispose();
                _disposed = true;
            }
        }

        ~BoxedValue()
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

        public TypeInfo TypeInfo
        {
            get
            {
                return _data.TypeInfo;
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

        public ref BoxedValueInternal Buffer
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
                throw new ObjectDisposedException(nameof(BoxedValue));
            }

            return _data.GetValue();
        }

        public void SetValue(object? value)
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(BoxedValue));
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

        public bool IsArray => BoxedValueInternal.BoxedValue_IsArray(ref _data);

        public int GetArraySize()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(BoxedValue));

            if (!BoxedValueInternal.BoxedValue_GetArraySize(ref _data, out int size))
                throw new InvalidOperationException("BoxedValue is not an array");

            return size;
        }

        public TypeInfo GetArrayElementTypeInfo()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(BoxedValue));

            IntPtr pTypeInfo = BoxedValueInternal.BoxedValue_GetArrayElemTypeInfo(ref _data);
            return new TypeInfo(pTypeInfo);
        }

        public unsafe BoxedValue GetArrayElement(int index)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(BoxedValue));

            BoxedValueInternal elem;
            if (!BoxedValueInternal.BoxedValue_GetArrayElem(ref _data, index, &elem))
                throw new InvalidOperationException($"Failed to get array element at index {index}");

            return FromBuffer(elem);
        }

        public void SetArrayElement(int index, BoxedValue value)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(BoxedValue));

            if (value == null)
                throw new ArgumentNullException(nameof(value));

            if (!BoxedValueInternal.BoxedValue_SetArrayElem(ref _data, index, ref value._data))
                throw new InvalidOperationException($"Failed to set array element at index {index}");
        }

        public void PushBackArrayElement(BoxedValue value)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(BoxedValue));

            if (value == null)
                throw new ArgumentNullException(nameof(value));

            if (!BoxedValueInternal.BoxedValue_PushBackArrayElem(ref _data, ref value._data))
                throw new InvalidOperationException("Failed to push back array element");
        }

        public void ResizeArray(int newSize)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(BoxedValue));

            if (!BoxedValueInternal.BoxedValue_ResizeArray(ref _data, newSize))
                throw new InvalidOperationException($"Failed to resize array to {newSize}");
        }

        public void RemoveArrayElement(int index)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(BoxedValue));

            if (!BoxedValueInternal.BoxedValue_RemoveArrayElement(ref _data, index))
                throw new InvalidOperationException($"Failed to remove array element at index {index}");
        }
    }
}