using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Reflection;

namespace Hyperion
{
    [Flags]
    public enum HypClassFlags : uint
    {
        None = 0x0,
        ClassType = 0x1,
        StructType = 0x2,
        EnumType = 0x4
    }

    public struct HypClass
    {
        public static readonly HypClass Invalid = new HypClass(IntPtr.Zero);
        private static readonly Dictionary<string, HypClass> hypClassCache = new Dictionary<string, HypClass>();
        private static readonly object hypClassCacheLock = new object();

        private IntPtr ptr;

        internal HypClass(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        internal IntPtr Address
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
                Name name = new Name(0);
                HypClass_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypClass_GetTypeID(ptr, out typeId);
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

        public IEnumerable<HypClassAttribute> Attributes
        {
            get
            {
                IntPtr attributesPtr;
                uint count = HypClass_GetAttributes(ptr, out attributesPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr attributePtr = Marshal.ReadIntPtr(attributesPtr, i * IntPtr.Size);
                    yield return new HypClassAttribute(attributePtr);
                }
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

        /// <summary>
        ///  Creates a native instance of this object. The object has an initial ref count of zero,
        ///  so the reference must be manually decremented to destroy the object (with HypObject_DecRef)  
        /// </summary>
        public IntPtr CreateInstance()
        {
            IntPtr instancePtr = HypClass_CreateInstance(ptr);

            if (instancePtr == IntPtr.Zero)
            {
                throw new Exception("Failed to initialize HypObject");
            }

            return instancePtr;
        }

        public IntPtr InitInstance(IntPtr classObjectPtr, ObjectReference objectReference)
        {
            IntPtr instancePtr = HypClass_InitInstance(ptr, classObjectPtr, ref objectReference);

            if (instancePtr == IntPtr.Zero)
            {
                throw new Exception("Failed to initialize HypObject");
            }

            return instancePtr;
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

            lock (hypClassCacheLock)
            {
                if (hypClassCache.TryGetValue(name, out HypClass foundHypClass))
                {
                    hypClass = foundHypClass;
                }
                else
                {
                    IntPtr ptr = HypClass_GetClassByName(name);

                    if (ptr != IntPtr.Zero)
                    {
                        hypClass = new HypClass(ptr);
                        hypClassCache[name] = hypClass.Value;
                    }
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
            HypClassBinding? hypClassBindingAttribute = HypClassBinding.ForType(type);

            if (hypClassBindingAttribute == null)
            {
                throw new InvalidOperationException($"Type {type.Name} is not a HypObject");
            }

            return ((HypClassBinding)hypClassBindingAttribute).HypClass;
        }

        public static HypClass? TryGetClass<T>()
        {
            return TryGetClass(typeof(T));
        }

        public static HypClass? TryGetClass(Type type)
        {
            HypClassBinding? hypClassBindingAttribute = HypClassBinding.ForType(type);

            if (hypClassBindingAttribute == null)
            {
                return null;
            }

            return hypClassBindingAttribute.HypClass;
        }
        
        [DllImport("hyperion", EntryPoint = "HypClass_CreateInstance")]
        private static extern IntPtr HypClass_CreateInstance([In] IntPtr hypClassPtr);
        
        [DllImport("hyperion", EntryPoint = "HypClass_InitInstance")]
        private static extern IntPtr HypClass_InitInstance([In] IntPtr hypClassPtr, [In] IntPtr classObjectPtr, [In] ref ObjectReference objectReference);

        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetName")]
        private static extern void HypClass_GetName([In] IntPtr hypClassPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetTypeID")]
        private static extern void HypClass_GetTypeID([In] IntPtr hypClassPtr, [Out] out TypeID typeId);
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetSize")]
        private static extern uint HypClass_GetSize([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetFlags")]
        private static extern uint HypClass_GetFlags([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetAttributes")]
        private static extern uint HypClass_GetAttributes([In] IntPtr hypClassPtr, [Out] out IntPtr outAttributesPtr);

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
    }
}