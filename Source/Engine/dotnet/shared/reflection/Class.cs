using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Reflection;
using System.Diagnostics;

namespace Hyperion
{
    [Flags]
    public enum ClassFlags : uint
    {
        None = 0x0,
        ClassType = 0x1,
        StructType = 0x2,
        EnumType = 0x4,
        Abstract = 0x8,
        PODType = 0x10,
        Dynamic = 0x20
    }

    public enum ClassAllocationMethod : byte
    {
        Invalid = 0xFF,

        None = 0,
        Handle = 1
    }

    public struct Class
    {
        public static readonly Class Invalid = new Class(IntPtr.Zero);
        private static readonly ConcurrentDictionary<string, Class> classTypeNameCache = new ConcurrentDictionary<string, Class>();
        private static readonly ConcurrentDictionary<Type, Class> classTypeObjectCache = new ConcurrentDictionary<Type, Class>();

        private IntPtr ptr;

        public Class(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public IntPtr Address
        {
            get => ptr;
            set => ptr = value;
        }

        public bool IsValid => ptr != IntPtr.Zero;

        public Name Name
        {
            get
            {
                Name name;
                Class_GetName(ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                Class_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        public TypeInfo TypeInfo
        {
            get
            {
                TypeInfo typeInfo;
                Class_GetTypeInfo(ptr, out typeInfo);
                return typeInfo;
            }
        }

        public uint Size => Class_GetSize(ptr);

        public ClassFlags Flags => (ClassFlags)Class_GetFlags(ptr);

        public bool IsClassType => (Flags & ClassFlags.ClassType) != 0;
        public bool IsStructType => (Flags & ClassFlags.StructType) != 0;
        public bool IsEnumType => (Flags & ClassFlags.EnumType) != 0;
        public bool IsAbstract => (Flags & ClassFlags.Abstract) != 0;
        public bool IsPODType => (Flags & ClassFlags.PODType) != 0;
        public bool IsDynamic => (Flags & ClassFlags.Dynamic) != 0;

        public ClassAllocationMethod AllocationMethod => (ClassAllocationMethod)Class_GetAllocationMethod(ptr);

        public bool IsReferenceCounted
        {
            get
            {
                ClassAllocationMethod allocationMethod = AllocationMethod;

                if (allocationMethod == ClassAllocationMethod.Handle)
                {
                    return true;
                }

                return false;
            }
        }

        public Class? GetParent()
        {
            IntPtr parentPtr = Class_GetParent(ptr);

            if (parentPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Class(parentPtr);
        }

        public bool IsSubclassOf(Class other)
        {
            Class? current = GetParent();

            while (current.HasValue)
            {
                if (current.Value == other)
                {
                    return true;
                }

                current = current.Value.GetParent();
            }

            return false;
        }

        public ClassAttribute? GetAttribute(Name name)
        {
            IntPtr attributePtr = Class_GetAttribute(ptr, ref name);

            if (attributePtr == IntPtr.Zero)
            {
                return null;
            }

            return new ClassAttribute(attributePtr);
        }

        public IEnumerable<ClassAttribute> Attributes
        {
            get
            {
                uint count = Class_GetAttributes(ptr, IntPtr.Zero);

                IntPtr attributesPtr = Marshal.AllocHGlobal(Marshal.SizeOf<IntPtr>() * (int)count);
                try
                {
                    IntPtr currentPtr = attributesPtr;

                    for (uint i = 0; i < count; i++)
                    {
                        yield return new ClassAttribute(Marshal.ReadIntPtr(currentPtr, (int)i * IntPtr.Size));
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(attributesPtr);
                }
            }
        }

        public IEnumerable<Property> Properties
        {
            get
            {
                uint count = Class_GetProperties(ptr, IntPtr.Zero);
                
                IntPtr propertyPtrs = Marshal.AllocHGlobal(IntPtr.Size * (int)count);

                try
                {
                    Class_GetProperties(ptr, propertyPtrs);

                    for (int i = 0; i < count; i++)
                    {
                        yield return new Property(Marshal.ReadIntPtr(propertyPtrs, i * IntPtr.Size));
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(propertyPtrs);
                }
            }
        }

        public Property? GetProperty(Name name)
        {
            IntPtr propertyPtr = Class_GetProperty(ptr, ref name);

            if (propertyPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Property(propertyPtr);
        }

        public IEnumerable<Method> Methods
        {
            get
            {
                uint count = Class_GetMethods(ptr, IntPtr.Zero);
                
                IntPtr methodPtrs = Marshal.AllocHGlobal(IntPtr.Size * (int)count);

                try
                {
                    Class_GetMethods(ptr, methodPtrs);

                    for (int i = 0; i < count; i++)
                    {
                        yield return new Method(Marshal.ReadIntPtr(methodPtrs, i * IntPtr.Size));
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(methodPtrs);
                }
            }
        }

        public Method? GetMethod(Name name)
        {
            IntPtr methodPtr = Class_GetMethod(ptr, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Method(methodPtr);
        }

        public IEnumerable<Field> Fields
        {
            get
            {
                uint count = Class_GetFields(ptr, IntPtr.Zero);
                
                IntPtr fieldPtrs = Marshal.AllocHGlobal(IntPtr.Size * (int)count);

                try
                {
                    Class_GetFields(ptr, fieldPtrs);

                    for (int i = 0; i < count; i++)
                    {
                        yield return new Field(Marshal.ReadIntPtr(fieldPtrs, i * IntPtr.Size));
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(fieldPtrs);
                }
            }
        }

        public Field? GetField(Name name)
        {
            IntPtr fieldPtr = Class_GetField(ptr, ref name);

            if (fieldPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Field(fieldPtr);
        }

        public IEnumerable<StaticField> StaticFields
        {
            get
            {
                uint count = Class_GetStaticFields(ptr, IntPtr.Zero);
                
                IntPtr staticFieldPtrs = Marshal.AllocHGlobal(IntPtr.Size * (int)count);

                try
                {
                    Class_GetStaticFields(ptr, staticFieldPtrs);

                    for (int i = 0; i < count; i++)
                    {
                        yield return new StaticField(Marshal.ReadIntPtr(staticFieldPtrs, i * IntPtr.Size));
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(staticFieldPtrs);
                }
            }
        }

        public StaticField? GetStaticField(Name name)
        {
            IntPtr constantPtr = Class_GetStaticField(ptr, ref name);

            if (constantPtr == IntPtr.Zero)
            {
                return null;
            }

            return new StaticField(constantPtr);
        }

        public void ValidateType(Type type)
        {
            if (!IsValid)
            {
                throw new Exception("Invalid Class");
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
                throw new Exception("Invalid Class type");
            }

            ClassAttribute? sizeAttribute = GetAttribute("size");

            if (sizeAttribute != null)
            {
                int size = sizeAttribute.Value.GetInt();

                if (size != Marshal.SizeOf(type))
                {
                    throw new Exception($"Struct size mismatch: Class struct size ({size}) does not match C# struct size ({Marshal.SizeOf(type)})");
                }
            }

            // Validate that all fields from the struct are present in the Class
            foreach (FieldInfo fieldInfo in type.GetFields())
            {
                Field? field = GetField(new Name(fieldInfo.Name));

                if (field == null)
                {
                    throw new Exception($"Field {fieldInfo.Name} not found in Class");
                }

                if ((int)field.Value.Offset != Marshal.OffsetOf(type, fieldInfo.Name).ToInt32())
                {
                    throw new Exception($"Field {fieldInfo.Name} offset mismatch: Class offset ({field.Value.Offset}) does not match C# offset ({Marshal.OffsetOf(type, fieldInfo.Name).ToInt32()})");
                }
            }
        }

        public static bool operator==(Class a, Class b)
        {
            return a.ptr == b.ptr;
        }

        public static bool operator!=(Class a, Class b)
        {
            return a.ptr != b.ptr;
        }

        public static Class GetClass<T>()
        {
            return GetClass(typeof(T));
        }

        public static Class GetClass(Type type)
        {
            Class? cls = TryGetClass(type);

            if (cls == null)
            {
                throw new Exception("Failed to get Class for type " + type.Name);
            }

            return (Class)cls;
        }

        public static Class? TryGetClass<T>()
        {
            return TryGetClass(typeof(T));
        }

        public static Class? TryGetClass(Type type)
        {
            if (classTypeObjectCache.TryGetValue(type, out Class foundClass))
                return foundClass;

            Type? currentType = type;

            while (true)
            {
                Debug.Assert(currentType != null);

                Attribute? attribute = Attribute.GetCustomAttribute(currentType, typeof(ClassBinding));

                if (attribute != null)
                    break;

                currentType = currentType.BaseType;

                if (currentType == null)
                    return null;
            }
            
            Assembly assembly = currentType.Assembly;

            ObjectReference assemblyObjectReference = new ObjectReference
            {
                WeakHandle = GCHandle.ToIntPtr(GCHandle.Alloc(assembly, GCHandleType.Weak)),
                StrongHandle = GCHandle.ToIntPtr(GCHandle.Alloc(assembly, GCHandleType.Normal))
            };

            IntPtr classPtr = IntPtr.Zero;

            unsafe
            {
                void* assemblyPtr;
                NativeInterop_GetAssemblyPointer(&assemblyObjectReference, &assemblyPtr);

                assemblyObjectReference.Dispose();

                if (assemblyPtr == null)
                    return null;

                classPtr = Class_GetClassByTypeHash((IntPtr)assemblyPtr, currentType.GetHashCode());
            }

            if (classPtr == IntPtr.Zero)
                return null;

            Class cls = new Class(classPtr);

            classTypeObjectCache[type] = cls;

            return cls;
        }

        public static Class? TryGetClass(TypeId typeId)
        {
            IntPtr ptr = Class_GetClassByTypeId(ref typeId);

            if (ptr == IntPtr.Zero)
            {
                return null;
            }

            return new Class(ptr);
        }
        
        public static Class? TryGetClass(string name)
        {
            Class? cls = null;

            if (classTypeNameCache.TryGetValue(name, out Class foundClass))
            {
                cls = foundClass;
            }
            else
            {
                IntPtr ptr = Class_GetClassByName(name);

                if (ptr != IntPtr.Zero)
                {
                    cls = new Class(ptr);
                    classTypeNameCache[name] = cls.Value;
                }
            }

            return cls;
        }

        [DllImport("hyperion", EntryPoint = "NativeInterop_GetAssemblyPointer")]
        private static extern unsafe void NativeInterop_GetAssemblyPointer([In] void* assemblyObjectReferencePtr, [Out] void* outAssemblyPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetClassByTypeId")]
        private static extern IntPtr Class_GetClassByTypeId([In] ref TypeId typeId);

        [DllImport("hyperion", EntryPoint = "Class_GetClassByName")]
        private static extern IntPtr Class_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "Class_GetClassByTypeHash")]
        private static extern IntPtr Class_GetClassByTypeHash([In] IntPtr assemblyPtr, int typeHash);
        
        [DllImport("hyperion", EntryPoint = "Class_GetName")]
        private static extern void Class_GetName([In] IntPtr classPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "Class_GetTypeId")]
        private static extern void Class_GetTypeId([In] IntPtr classPtr, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "Class_GetTypeInfo")]
        private static extern void Class_GetTypeInfo([In] IntPtr classPtr, [Out] out TypeInfo typeInfo);
        
        [DllImport("hyperion", EntryPoint = "Class_GetSize")]
        private static extern uint Class_GetSize([In] IntPtr classPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetFlags")]
        private static extern uint Class_GetFlags([In] IntPtr classPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetAllocationMethod")]
        private static extern byte Class_GetAllocationMethod([In] IntPtr classPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetParent")]
        private static extern IntPtr Class_GetParent([In] IntPtr classPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetAttribute")]
        private static extern IntPtr Class_GetAttribute([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "Class_GetAttributes")]
        private static extern uint Class_GetAttributes([In] IntPtr classPtr, [Out] IntPtr attributesPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetProperties")]
        private static extern uint Class_GetProperties([In] IntPtr classPtr, [Out] IntPtr propertiesPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetProperty")]
        private static extern IntPtr Class_GetProperty([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "Class_GetMethods")]
        private static extern uint Class_GetMethods([In] IntPtr classPtr, [Out] IntPtr methodsPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetMethod")]
        private static extern IntPtr Class_GetMethod([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "Class_GetFields")]
        private static extern uint Class_GetFields([In] IntPtr classPtr, [Out] IntPtr fieldsPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetField")]
        private static extern IntPtr Class_GetField([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "Class_GetStaticFields")]
        private static extern uint Class_GetStaticFields([In] IntPtr classPtr, [Out] IntPtr staticFieldsPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetStaticField")]
        private static extern IntPtr Class_GetStaticField([In] IntPtr classPtr, [In] ref Name name);
    }
}