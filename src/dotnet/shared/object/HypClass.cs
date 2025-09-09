using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Reflection;

namespace Hyperion
{
    [Flags]
    public enum HypClassFlags : uint
    {
        None = 0x0,
        ClassType = 0x1,
        StructType = 0x2,
        EnumType = 0x4,
        Abstract = 0x8,
        PODType = 0x10,
        Dynamic = 0x20
    }

    public enum HypClassAllocationMethod : byte
    {
        Invalid = 0xFF,

        None = 0,
        Handle = 1,
        RefCountedPtr = 2
    }

    public struct HypClass
    {
        public static readonly HypClass Invalid = new HypClass(IntPtr.Zero);
        private static readonly ConcurrentDictionary<string, HypClass> hypClassTypeNameCache = new ConcurrentDictionary<string, HypClass>();
        private static readonly ConcurrentDictionary<Type, HypClass> hypClassTypeObjectCache = new ConcurrentDictionary<Type, HypClass>();

        private IntPtr ptr;

        public HypClass(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
            set
            {
                ptr = value;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Name Name
        {
            get
            {
                Name name;
                HypClass_GetName(ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                HypClass_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        public uint Size
        {
            get
            {
                return HypClass_GetSize(ptr);
            }
        }

        public HypClassFlags Flags
        {
            get
            {
                return (HypClassFlags)HypClass_GetFlags(ptr);
            }
        }

        public bool IsClassType
        {
            get
            {
                return (Flags & HypClassFlags.ClassType) != 0;
            }
        }

        public bool IsStructType
        {
            get
            {
                return (Flags & HypClassFlags.StructType) != 0;
            }
        }

        public bool IsEnumType
        {
            get
            {
                return (Flags & HypClassFlags.EnumType) != 0;
            }
        }

        public bool IsAbstract
        {
            get
            {
                return (Flags & HypClassFlags.Abstract) != 0;
            }
        }

        public bool is_pod_type
        {
            get
            {
                return (Flags & HypClassFlags.PODType) != 0;
            }
        }

        public bool IsDynamic
        {
            get
            {
                return (Flags & HypClassFlags.Dynamic) != 0;
            }
        }

        public HypClassAllocationMethod AllocationMethod
        {
            get
            {
                return (HypClassAllocationMethod)HypClass_GetAllocationMethod(ptr);
            }
        }

        public bool IsReferenceCounted
        {
            get
            {
                HypClassAllocationMethod allocationMethod = AllocationMethod;

                if (allocationMethod == HypClassAllocationMethod.Handle || allocationMethod == HypClassAllocationMethod.RefCountedPtr)
                {
                    return true;
                }

                return false;
            }
        }

        public HypClassAttribute? GetAttribute(string name)
        {
            IntPtr attributePtr = HypClass_GetAttribute(ptr, name);

            if (attributePtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypClassAttribute(attributePtr);
        }

        public IEnumerable<HypProperty> Properties
        {
            get
            {
                IntPtr propertiesPtr;
                uint count = HypClass_GetProperties(ptr, out propertiesPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr propertyPtr = Marshal.ReadIntPtr(propertiesPtr, i * IntPtr.Size);
                    yield return new HypProperty(propertyPtr);
                }
            }
        }

        public HypProperty? GetProperty(Name name)
        {
            IntPtr propertyPtr = HypClass_GetProperty(ptr, ref name);

            if (propertyPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypProperty(propertyPtr);
        }

        public IEnumerable<HypMethod> Methods
        {
            get
            {
                IntPtr methodsPtr;
                uint count = HypClass_GetMethods(ptr, out methodsPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr methodPtr = Marshal.ReadIntPtr(methodsPtr, i * IntPtr.Size);
                    yield return new HypMethod(methodPtr);
                }
            }
        }

        public HypMethod? GetMethod(Name name)
        {
            IntPtr methodPtr = HypClass_GetMethod(ptr, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypMethod(methodPtr);
        }

        public IEnumerable<HypField> Fields
        {
            get
            {
                IntPtr fieldsPtr;
                uint count = HypClass_GetFields(ptr, out fieldsPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr fieldPtr = Marshal.ReadIntPtr(fieldsPtr, i * IntPtr.Size);
                    yield return new HypField(fieldPtr);
                }
            }
        }

        public HypField? GetField(Name name)
        {
            IntPtr fieldPtr = HypClass_GetField(ptr, ref name);

            if (fieldPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypField(fieldPtr);
        }

        public IEnumerable<HypConstant> Constants
        {
            get
            {
                IntPtr constantsPtr;
                uint count = HypClass_GetConstants(ptr, out constantsPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr constantPtr = Marshal.ReadIntPtr(constantsPtr, i * IntPtr.Size);
                    yield return new HypConstant(constantPtr);
                }
            }
        }

        public HypConstant? GetConstant(Name name)
        {
            IntPtr constantPtr = HypClass_GetConstant(ptr, ref name);

            if (constantPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypConstant(constantPtr);
        }

        public void ValidateType(Type type)
        {
            if (!IsValid)
            {
                throw new Exception("Invalid HypClass");
            }

            if (IsStructType)
            {
                if (!type.IsValueType)
                {
                    throw new Exception("Expected a struct type");
                }
            }
            else if (IsClassType)
            {
                if (!type.IsClass)
                {
                    throw new Exception("Expected a class type");
                }
            }
            else if (IsEnumType)
            {
                if (!type.IsEnum)
                {
                    throw new Exception("Expected an enum type");
                }
            }
            else
            {
                throw new Exception("Invalid HypClass type");
            }

            HypClassAttribute? sizeAttribute = this.GetAttribute("size");

            if (sizeAttribute != null)
            {
                int size = sizeAttribute.Value.GetInt();

                if (size != Marshal.SizeOf(type))
                {
                    throw new Exception($"Struct size mismatch: HypClass struct size ({size}) does not match C# struct size ({Marshal.SizeOf(type)})");
                }
            }

            // Validate that all fields from the struct are present in the HypClass
            foreach (FieldInfo field in type.GetFields())
            {
                HypField? hypField = this.GetField(new Name(field.Name));

                if (hypField == null)
                {
                    throw new Exception($"Field {field.Name} not found in HypClass");
                }

                if ((int)hypField.Value.Offset != Marshal.OffsetOf(type, field.Name).ToInt32())
                {
                    throw new Exception($"Field {field.Name} offset mismatch: HypClass offset ({hypField.Value.Offset}) does not match C# offset ({Marshal.OffsetOf(type, field.Name).ToInt32()})");
                }
            }
        }

        public static bool operator==(HypClass a, HypClass b)
        {
            return a.ptr == b.ptr;
        }

        public static bool operator!=(HypClass a, HypClass b)
        {
            return a.ptr != b.ptr;
        }

        public static HypClass? GetClass(string name)
        {
            HypClass? hypClass = null;

            if (hypClassTypeNameCache.TryGetValue(name, out HypClass foundHypClass))
            {
                hypClass = foundHypClass;
            }
            else
            {
                IntPtr ptr = HypClass_GetClassByName(name);

                if (ptr != IntPtr.Zero)
                {
                    hypClass = new HypClass(ptr);
                    hypClassTypeNameCache[name] = hypClass.Value;
                }
            }

            return hypClass;
        }

        public static HypClass GetClass<T>()
        {
            return GetClass(typeof(T));
        }

        public static HypClass GetClass(Type type)
        {
            HypClass? hypClass = TryGetClass(type);

            if (hypClass == null)
            {
                throw new Exception("Failed to get HypClass for type " + type.Name);
            }

            return (HypClass)hypClass;
        }

        public static HypClass? TryGetClass<T>()
        {
            return TryGetClass(typeof(T));
        }

        public static HypClass? TryGetClass(Type type)
        {
            if (hypClassTypeObjectCache.TryGetValue(type, out HypClass foundHypClass))
                return foundHypClass;

            Type? currentType = type;

            while (true)
            {
                Attribute? attribute = Attribute.GetCustomAttribute((Type)currentType, typeof(HypClassBinding));

                if (attribute != null)
                    break;

                currentType = ((Type)currentType).BaseType;

                if (currentType == null)
                    return null;
            }
            
            Assembly assembly = currentType.Assembly;

            ObjectReference assemblyObjectReference = new ObjectReference
            {
                weakHandle = GCHandle.ToIntPtr(GCHandle.Alloc(assembly, GCHandleType.Weak)),
                strongHandle = GCHandle.ToIntPtr(GCHandle.Alloc(assembly, GCHandleType.Normal))
            };

            IntPtr hypClassPtr = IntPtr.Zero;

            unsafe
            {
                void* assemblyPtr;
                NativeInterop_GetAssemblyPointer(&assemblyObjectReference, &assemblyPtr);

                assemblyObjectReference.Dispose();

                if (assemblyPtr == null)
                    return null;

                hypClassPtr = HypClass_GetClassByTypeHash((IntPtr)assemblyPtr, currentType.GetHashCode());
            }

            if (hypClassPtr == IntPtr.Zero)
                return null;

            HypClass hypClass = new HypClass(hypClassPtr);

            hypClassTypeObjectCache[type] = hypClass;

            return hypClass;
        }

        [DllImport("hyperion", EntryPoint = "NativeInterop_GetAssemblyPointer")]
        private static extern unsafe void NativeInterop_GetAssemblyPointer([In] void* assemblyObjectReferencePtr, [Out] void* outAssemblyPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByTypeHash")]
        private static extern IntPtr HypClass_GetClassByTypeHash([In] IntPtr assemblyPtr, int typeHash);
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetName")]
        private static extern void HypClass_GetName([In] IntPtr hypClassPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetTypeId")]
        private static extern void HypClass_GetTypeId([In] IntPtr hypClassPtr, [Out] out TypeId typeId);
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetSize")]
        private static extern uint HypClass_GetSize([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetFlags")]
        private static extern uint HypClass_GetFlags([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetAllocationMethod")]
        private static extern byte HypClass_GetAllocationMethod([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetAttribute")]
        private static extern IntPtr HypClass_GetAttribute([In] IntPtr hypClassPtr, [MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetProperties")]
        private static extern uint HypClass_GetProperties([In] IntPtr hypClassPtr, [Out] out IntPtr outPropertiesPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetProperty")]
        private static extern IntPtr HypClass_GetProperty([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetMethods")]
        private static extern uint HypClass_GetMethods([In] IntPtr hypClassPtr, [Out] out IntPtr outMethodsPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetMethod")]
        private static extern IntPtr HypClass_GetMethod([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetFields")]
        private static extern uint HypClass_GetFields([In] IntPtr hypClassPtr, [Out] out IntPtr outFieldsPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetField")]
        private static extern IntPtr HypClass_GetField([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetConstants")]
        private static extern uint HypClass_GetConstants([In] IntPtr hypClassPtr, [Out] out IntPtr outConstantsPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetConstant")]
        private static extern IntPtr HypClass_GetConstant([In] IntPtr hypClassPtr, [In] ref Name name);
    }
}