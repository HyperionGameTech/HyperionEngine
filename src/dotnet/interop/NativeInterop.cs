using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public delegate void InvokeMethodDelegate(Guid managedMethodGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr);
    public delegate void InitializeObjectCallbackDelegate(IntPtr contextPtr, IntPtr objectPtr, uint objectSize);
    public delegate void AddObjectToCacheDelegate(IntPtr objectWrapperPtr, IntPtr outObjectReferencePtr);

    internal enum LoadAssemblyResult : int
    {
        UnknownError = -100,
        VersionMismatch = -2,
        NotFound = -1,
        Ok = 0
    }
    
    public class NativeInterop
    {
        private static bool VerifyEngineVersion(string versionString, bool major, bool minor, bool patch)
        {
            var versionParts = versionString.Split('.');

            uint majorVersion = versionParts.Length > 0 ? uint.Parse(versionParts[0]) : 0;
            uint minorVersion = versionParts.Length > 1 ? uint.Parse(versionParts[1]) : 0;
            uint patchVersion = versionParts.Length > 2 ? uint.Parse(versionParts[2]) : 0;

            uint assemblyEngineVersion = (majorVersion << 16) | (minorVersion << 8) | patchVersion;

            // Verify the engine version (major, minor)
            return NativeInterop_VerifyEngineVersion(assemblyEngineVersion, major, minor, patch);
        }

        [UnmanagedCallersOnly]
        public static int InitializeAssembly(IntPtr outAssemblyGuid, IntPtr classHolderPtr, IntPtr assemblyPathStringPtr, int isCoreAssembly)
        {
            try
            {
                // Create a managed string from the pointer
                string assemblyPath = Marshal.PtrToStringAnsi(assemblyPathStringPtr);

                if (isCoreAssembly != 0)
                {
                    // Check for assemblies having already been loaded
                    if (AssemblyCache.Instance.Get(assemblyPath) != null)
                    {
                        // throw new Exception("Assembly already loaded: " + assemblyPath + " (" + AssemblyCache.Instance.Get(assemblyPath).Guid + ")");

                        Logger.Log(LogType.Info, "Assembly already loaded: {0}", assemblyPath);

                        return (int)LoadAssemblyResult.Ok;
                    }
                }

                Guid assemblyGuid = Guid.NewGuid();
                Marshal.StructureToPtr(assemblyGuid, outAssemblyGuid, false);

                AssemblyInstance assemblyInstance = AssemblyCache.Instance.Add(assemblyGuid, assemblyPath, classHolderPtr, isCoreAssembly: isCoreAssembly != 0);
                Assembly? assembly = assemblyInstance.Assembly;

                if (assembly == null)
                {
                    Logger.Log(LogType.Error, "Failed to load assembly: " + assemblyPath);

                    return (int)LoadAssemblyResult.NotFound;
                }

                AssemblyName hyperionCoreDependency = Array.Find(assembly.GetReferencedAssemblies(), (assemblyName) => assemblyName.Name == "HyperionCore");

                if (hyperionCoreDependency != null)
                {
                    // Verify the engine version (major, minor)
                    if (!VerifyEngineVersion(hyperionCoreDependency.Version.ToString(), true, true, false))
                    {
                        Logger.Log(LogType.Error, "Assembly version does not match engine version");

                        return (int)LoadAssemblyResult.VersionMismatch;
                    }
                }

                NativeInterop_SetInvokeMethodFunction(ref assemblyGuid, classHolderPtr, Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeMethod));
                NativeInterop_SetInvokeGetterFunction(ref assemblyGuid, classHolderPtr, Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeGetter));
                NativeInterop_SetInvokeSetterFunction(ref assemblyGuid, classHolderPtr, Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeSetter));

                NativeInterop_SetAddObjectToCacheFunction(Marshal.GetFunctionPointerForDelegate<AddObjectToCacheDelegate>(AddObjectToCache));

                Type[] types = assembly.GetTypes();

                foreach (Type type in types)
                {
                    if (type.IsClass || (type.IsValueType && !type.IsEnum))
                    {
                        // Register classes and structs
                        InitManagedClass(type);
                    }
                }
            }
            catch (Exception err)
            {
                Logger.Log(LogType.Error, "Error loading assembly: {0}", err);

                return (int)LoadAssemblyResult.UnknownError;
            }

            return (int)LoadAssemblyResult.Ok;
        }
        
        [UnmanagedCallersOnly]
        public static void UnloadAssembly(IntPtr assemblyGuidPtr, IntPtr outResult)
        {
            bool result = true;

            try
            {
                Guid assemblyGuid = Marshal.PtrToStructure<Guid>(assemblyGuidPtr);

                if (AssemblyCache.Instance.Get(assemblyGuid) == null)
                {
                    Logger.Log(LogType.Warn, "Failed to unload assembly: {0} not found", assemblyGuid);

                    foreach (var kv in AssemblyCache.Instance.Assemblies)
                    {
                        Logger.Log(LogType.Info, "Assembly: {0}", kv.Key);
                    }

                    result = false;
                }
                else
                {
                    Logger.Log(LogType.Info, "Unloading assembly: {0}...", assemblyGuid);

                    AssemblyCache.Instance.Remove(assemblyGuid);
                }
            }
            catch (Exception err)
            {
                Logger.Log(LogType.Error, "Error unloading assembly: {0}", err);

                result = false;
            }

            Marshal.WriteInt32(outResult, result ? 1 : 0);
        }

        private static unsafe ManagedAttributeHolder AllocAttributeHolder(Guid assemblyGuid, IntPtr classHolderPtr, object[] attributes)
        {
            if (attributes.Length == 0)
            {
                return new ManagedAttributeHolder
                {
                    managedAttributesSize = 0,
                    managedAttributesPtr = IntPtr.Zero
                };
            }

            ManagedAttributeHolder managedAttributeHolder = new ManagedAttributeHolder
            {
                managedAttributesSize = (uint)attributes.Length,
                managedAttributesPtr = Marshal.AllocHGlobal(Marshal.SizeOf<ManagedAttribute>() * attributes.Length)
            };

            for (int i = 0; i < attributes.Length; i++)
            {
                object attribute = attributes[i];
                Type attributeType = attribute.GetType();

                IntPtr attributeManagedClassObjectPtr = InitManagedClass(attributeType);

                // add the attribute to the object cache
                Guid attributeGuid = Guid.NewGuid();
                ObjectReference attributeObjectReference = ManagedObjectCache.Instance.AddObject(assemblyGuid, attributeGuid, attribute, false, null);

                ref ManagedAttribute managedAttribute = ref Unsafe.AsRef<ManagedAttribute>((void*)(managedAttributeHolder.managedAttributesPtr + (i * Marshal.SizeOf<ManagedAttribute>())));
                managedAttribute.classObjectPtr = attributeManagedClassObjectPtr;
                managedAttribute.objectReference = attributeObjectReference;
            }

            return managedAttributeHolder;
        }

        private static Dictionary<string, MethodInfo> CollectMethods(Type type)
        {
            Dictionary<string, MethodInfo> methods = new Dictionary<string, MethodInfo>();

            CollectMethods(type, methods);

            return methods;
        }

        private static void CollectMethods(Type type, Dictionary<string, MethodInfo> methods)
        {
            MethodInfo[] methodInfos = type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.FlattenHierarchy);

            foreach (MethodInfo methodInfo in methodInfos)
            {
                // Skip duplicates in hierarchy
                if (methods.ContainsKey(methodInfo.Name))
                {
                    continue;
                }

                // Skip constructors
                if (methodInfo.IsConstructor)
                {
                    continue;
                }

                methods.Add(methodInfo.Name, methodInfo);
            }

            if (type.BaseType != null)
            {
                CollectMethods(type.BaseType, methods);
            }
        }

        private static Dictionary<string, PropertyInfo> CollectProperties(Type type)
        {
            Dictionary<string, PropertyInfo> properties = new Dictionary<string, PropertyInfo>();

            CollectProperties(type, properties);

            return properties;
        }

        private static void CollectProperties(Type type, Dictionary<string, PropertyInfo> properties)
        {
            PropertyInfo[] propertyInfos = type.GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.FlattenHierarchy);

            foreach (PropertyInfo propertyInfo in propertyInfos)
            {
                // Skip duplicates in hierarchy
                if (properties.ContainsKey(propertyInfo.Name))
                {
                    continue;
                }

                properties.Add(propertyInfo.Name, propertyInfo);
            }

            if (type.BaseType != null)
            {
                CollectProperties(type.BaseType, properties);
            }
        }

        private static unsafe IntPtr InitManagedClass(Type type)
        {
            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.Get(type.Assembly);

            if (assemblyInstance == null)
            {
                throw new Exception("Failed to get assembly instance for type: " + type.Name + " from assembly: " + type.Assembly.FullName + ", has the assembly been registered?");
            }

            Guid assemblyGuid = assemblyInstance.Guid;
            IntPtr classHolderPtr = assemblyInstance.ClassHolderPtr;

            // @FIXME: Will not be able to find classes in other assemblies! 
            if (ManagedClass_FindByTypeHash(classHolderPtr, type.GetHashCode(), out IntPtr foundManagedClassObjectPtr))
            {
                return foundManagedClassObjectPtr;
            }

            Logger.Log(LogType.Debug, "Initializing managed class for type: {0}, Hash: {1}", type.Name, type.GetHashCode());

            ManagedClass managedClass = new ManagedClass();

            IntPtr parentClassObjectPtr = IntPtr.Zero;

            Type? baseType = type.BaseType;

            if (baseType != null)
            {
                parentClassObjectPtr = InitManagedClass(baseType);
            }

            string typeName = type.Name;
            IntPtr typeNamePtr = Marshal.StringToHGlobalAnsi(typeName);

            ManagedClassFlags managedClassFlags = ManagedClassFlags.None;

            if (type.IsClass)
            {
                managedClassFlags |= ManagedClassFlags.ClassType;
            }
            else if (type.IsValueType)
            {
                managedClassFlags |= ManagedClassFlags.StructType;
            }
            else if (type.IsEnum)
            {
                managedClassFlags |= ManagedClassFlags.EnumType;
            }

            IntPtr hypClassPtr = IntPtr.Zero;

            foreach (var attr in type.GetCustomAttributes(false))
            {
                if (attr.GetType().Name == "HypClassBinding")
                {
                    string hypClassName = (string)attr.GetType().GetProperty("Name").GetValue(attr);

                    if (hypClassName == null || hypClassName.Length == 0)
                    {
                        hypClassName = typeName;
                    }

                    hypClassPtr = HypClass_GetClassByName(hypClassName);

                    if (hypClassPtr == IntPtr.Zero)
                    {
                        throw new Exception(string.Format("No HypClass found for \"{0}\"", hypClassName));
                    }

                    Logger.Log(LogType.Debug, "Found HypClass for type: {0}, Name: {1}", typeName, hypClassName);

                    break;
                }
            }

            ManagedClass_Create(ref assemblyGuid, classHolderPtr, hypClassPtr, type.GetHashCode(), typeNamePtr, parentClassObjectPtr, (uint)managedClassFlags, out managedClass);

            Marshal.FreeHGlobal(typeNamePtr);

            ManagedAttributeHolder managedAttributeHolder = AllocAttributeHolder(assemblyGuid, classHolderPtr, type.GetCustomAttributes().ToArray());
            managedClass.SetAttributes(ref managedAttributeHolder);
            managedAttributeHolder.Dispose();
            
            foreach (var item in CollectMethods(type))
            {
                MethodInfo methodInfo = item.Value;
                
                managedAttributeHolder = AllocAttributeHolder(assemblyGuid, classHolderPtr, methodInfo.GetCustomAttributes(false));

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                Guid methodGuid = Guid.NewGuid();
                managedClass.AddMethod(item.Key, methodGuid, ref managedAttributeHolder);

                BasicCache<MethodInfo>.Instance.Add(assemblyGuid, methodGuid, methodInfo);

                managedAttributeHolder.Dispose();
            }

            foreach (var item in CollectProperties(type))
            {
                PropertyInfo propertyInfo = item.Value;

                managedAttributeHolder = AllocAttributeHolder(assemblyGuid, classHolderPtr, propertyInfo.GetCustomAttributes(false));

                Guid propertyGuid = Guid.NewGuid();
                managedClass.AddProperty(item.Key, propertyGuid, ref managedAttributeHolder);

                BasicCache<PropertyInfo>.Instance.Add(assemblyGuid, propertyGuid, propertyInfo);

                managedAttributeHolder.Dispose();
            }

            // Add new object, free object delegates
            managedClass.NewObjectFunction = new NewObjectDelegate((bool keepAlive, IntPtr hypClassPtr, IntPtr nativeAddress, IntPtr contextPtr, IntPtr callbackPtr) =>
            {
                // Allocate the object
                object obj = RuntimeHelpers.GetUninitializedObject(type);

                // Call the constructor
                ConstructorInfo? constructorInfo;
                object[]? parameters = null;

                if (hypClassPtr != IntPtr.Zero)
                {
                    if (nativeAddress == IntPtr.Zero)
                    {
                        throw new ArgumentNullException(nameof(nativeAddress));
                    }

                    Type objType = obj.GetType();

                    FieldInfo? hypClassPtrField = objType.GetField("_hypClassPtr", BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.FlattenHierarchy);
                    FieldInfo? nativeAddressField = objType.GetField("_nativeAddress", BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.FlattenHierarchy);

                    if (hypClassPtrField == null || nativeAddressField == null)
                    {
                        throw new InvalidOperationException("Could not find hypClassPtr or nativeAddress field on class " + type.Name);
                    }

                    hypClassPtrField.SetValue(obj, hypClassPtr);
                    nativeAddressField.SetValue(obj, nativeAddress);
                }

                constructorInfo = type.GetConstructor(BindingFlags.Public | BindingFlags.Instance, null, Type.EmptyTypes, null);

                if (constructorInfo == null)
                {
                    throw new InvalidOperationException("Failed to find empty constructor for type: " + type.Name);
                }

                constructorInfo.Invoke(obj, parameters);

                GCHandle? gcHandle = null;

                if (callbackPtr != IntPtr.Zero)
                {
                    if (!type.IsValueType)
                    {
                        throw new InvalidOperationException("InitializeObjectCallback can only be used with value types");
                    }

                    gcHandle = GCHandle.Alloc(obj, GCHandleType.Pinned);

                    InitializeObjectCallbackDelegate callbackDelegate = Marshal.GetDelegateForFunctionPointer<InitializeObjectCallbackDelegate>(callbackPtr);
                    callbackDelegate(contextPtr, ((GCHandle)gcHandle).AddrOfPinnedObject(), (uint)Marshal.SizeOf(type));

                    if (!keepAlive)
                    {
                        ((GCHandle)gcHandle).Free();
                        gcHandle = null;
                    }
                }

                Guid objectGuid = Guid.NewGuid();
                
                return ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj, keepAlive, gcHandle);
            });

            managedClass.FreeObjectFunction = new FreeObjectDelegate(FreeObject);

            managedClass.MarshalObjectFunction = new MarshalObjectDelegate((IntPtr ptr, uint size) =>
            {
                if (ptr == IntPtr.Zero)
                {
                    throw new ArgumentNullException(nameof(ptr));
                }

                if (size != Marshal.SizeOf(type))
                {
                    throw new ArgumentException("Size does not match type size", nameof(size));
                }

                // Marshal object from pointer
                object obj = Marshal.PtrToStructure(ptr, type);

                Guid objectGuid = Guid.NewGuid();

                return ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj, false, null);
            });

            return managedClass.ClassObjectPtr;
        }

        private static unsafe void HandleParameters(IntPtr paramsPtr, MethodInfo methodInfo, out object?[] parameters)
        {
            int numParams = methodInfo.GetParameters().Length;

            if (numParams == 0 || paramsPtr == IntPtr.Zero)
            {
                parameters = Array.Empty<object>();

                return;
            }

            parameters = new object[numParams];

            int paramsOffset = 0;

            for (int i = 0; i < numParams; i++)
            {
                Type paramType = methodInfo.GetParameters()[i].ParameterType;

                // params is stored as void**
                IntPtr paramAddress = Marshal.ReadIntPtr(paramsPtr, paramsOffset);
                paramsOffset += IntPtr.Size;

                MarshalHelpers.MarshalInObject(paramAddress, paramType, out parameters[i]);
            }
        }

        public static unsafe void InvokeMethod(Guid managedMethodGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr)
        {
            MethodInfo methodInfo = BasicCache<MethodInfo>.Instance.Get(managedMethodGuid);

            Type returnType = methodInfo.ReturnType;
            Type thisType = methodInfo.DeclaringType;

            object[]? parameters = null;
            HandleParameters(paramsPtr, methodInfo, out parameters);

            object? thisObject = null;

            if (!methodInfo.IsStatic)
            {
                thisObject = ManagedObjectCache.Instance.GetObject(thisObjectGuid);

                if (thisObject == null)
                {
                    throw new Exception("Failed to get target from GUID: " + thisObjectGuid);
                }
            }

            object? returnValue = methodInfo.Invoke(thisObject, parameters);

            MarshalHelpers.MarshalOutObject(outPtr, returnType, returnValue);
        }

        public static unsafe void InvokeGetter(Guid managedPropertyGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr)
        {
            PropertyInfo propertyInfo = BasicCache<PropertyInfo>.Instance.Get(managedPropertyGuid);

            Type returnType = propertyInfo.PropertyType;
            Type thisType = propertyInfo.DeclaringType;

            object? thisObject = ManagedObjectCache.Instance.GetObject(thisObjectGuid);

            if (thisObject == null)
            {
                throw new Exception("Failed to get target from GUID: " + thisObjectGuid);
            }

            object? returnValue = propertyInfo.GetValue((object)thisObject);

            MarshalHelpers.MarshalOutObject(outPtr, returnType, returnValue);
        }

        public static unsafe void InvokeSetter(Guid managedPropertyGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr)
        {
            PropertyInfo propertyInfo = BasicCache<PropertyInfo>.Instance.Get(managedPropertyGuid);

            Type returnType = propertyInfo.PropertyType;
            Type thisType = propertyInfo.DeclaringType;

            object? thisObject = ManagedObjectCache.Instance.GetObject(thisObjectGuid);

            if (thisObject == null)
            {
                throw new Exception("Failed to get target from GUID: " + thisObjectGuid);
            }

            object? value = null;
            MarshalHelpers.MarshalInObject(paramsPtr, propertyInfo.PropertyType, out value);

            propertyInfo.SetValue((object)thisObject, value);
        }

        /// <summary>
        /// Adds a managed object to the managed object cache. The object is not kept alive,
        /// as it is intended for objects that are already handled by the .NET runtime,
        /// with no UniquePtr<Object> in C++.
        /// </summary>
        public static unsafe void AddObjectToCache(IntPtr objectWrapperPtr, IntPtr outObjectReferencePtr)
        {
            ref ObjectWrapper objectWrapperRef = ref Unsafe.AsRef<ObjectWrapper>((void*)objectWrapperPtr);
            ref ObjectReference objectReferenceRef = ref Unsafe.AsRef<ObjectReference>((void*)outObjectReferencePtr);

            object obj = objectWrapperRef.obj;

            if (obj == null)
            {
                throw new ArgumentNullException(nameof(obj));
            }

            Type type = obj.GetType();

            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.Get(type.Assembly);

            if (assemblyInstance == null)
            {
                throw new Exception("Failed to get assembly instance for type: " + type.Name + " from assembly: " + type.Assembly.FullName + ", has the assembly been registered?");
            }

            Guid assemblyGuid = assemblyInstance.Guid;
            IntPtr classHolderPtr = assemblyInstance.ClassHolderPtr;

            Guid objectGuid = Guid.NewGuid();

            // reassign ref
            objectReferenceRef = ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj, keepAlive: false, gcHandle: null);
        }

        public static void FreeObject(ObjectReference obj)
        {
            if (!ManagedObjectCache.Instance.RemoveObject(obj.guid))
            {
                throw new Exception("Failed to remove object from cache: " + obj.guid);
            }
        }

        [DllImport("hyperion", EntryPoint = "ManagedClass_Create")]
        private static extern void ManagedClass_Create(ref Guid assemblyGuid, IntPtr classHolderPtr, IntPtr hypClassPtr, int typeHash, IntPtr typeNamePtr, IntPtr parentClassPtr, uint managedClassFlags, [Out] out ManagedClass result);

        [DllImport("hyperion", EntryPoint = "ManagedClass_FindByTypeHash")]
        private static extern bool ManagedClass_FindByTypeHash(IntPtr classHolderPtr, int typeHash, [Out] out IntPtr outManagedClassObjectPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "NativeInterop_VerifyEngineVersion")]
        private static extern bool NativeInterop_VerifyEngineVersion(uint assemblyEngineVersion, bool major, bool minor, bool patch);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetInvokeMethodFunction")]
        private static extern void NativeInterop_SetInvokeMethodFunction([In] ref Guid assemblyGuid, IntPtr classHolderPtr, IntPtr invokeMethodPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetInvokeGetterFunction")]
        private static extern void NativeInterop_SetInvokeGetterFunction([In] ref Guid assemblyGuid, IntPtr classHolderPtr, IntPtr invokeGetterPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetInvokeSetterFunction")]
        private static extern void NativeInterop_SetInvokeSetterFunction([In] ref Guid assemblyGuid, IntPtr classHolderPtr, IntPtr invokeSetterPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetAddObjectToCacheFunction")]
        private static extern void NativeInterop_SetAddObjectToCacheFunction(IntPtr addObjectToCachePtr);
    }
}
