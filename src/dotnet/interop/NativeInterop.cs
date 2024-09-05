using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public delegate void InvokeMethodDelegate(Guid managedMethodGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr);
    public delegate void InitializeObjectCallbackDelegate(IntPtr contextPtr, IntPtr objectPtr, uint objectSize);

    internal enum LoadAssemblyResult : int
    {
        UnknownError = -100,
        VersionMismatch = -2,
        NotFound = -1,
        Ok = 0
    }
    
    public class NativeInterop
    {
        public static InvokeMethodDelegate InvokeMethodDelegate = InvokeMethod;

        [UnmanagedCallersOnly]
        public static int InitializeAssembly(IntPtr outAssemblyGuid, IntPtr classHolderPtr, IntPtr assemblyPathStringPtr, int isCoreAssembly)
        {
            try
            {
                // Create a managed string from the pointer
                string assemblyPath = Marshal.PtrToStringAnsi(assemblyPathStringPtr);

                if (isCoreAssembly != 0)
                {
                    if (AssemblyCache.Instance.Get(assemblyPath) != null)
                    {
                        // throw new Exception("Assembly already loaded: " + assemblyPath + " (" + AssemblyCache.Instance.Get(assemblyPath).Guid + ")");

                        Logger.Log(LogType.Info, "Assembly already loaded: {0}", assemblyPath);

                        return (int)LoadAssemblyResult.Ok;
                    }
                }

                Guid assemblyGuid = Guid.NewGuid();
                Marshal.StructureToPtr(assemblyGuid, outAssemblyGuid, false);

                Logger.Log(LogType.Info, "Loading assembly: {0}...", assemblyPath);
                
                Assembly? assembly = null;

                if (isCoreAssembly != 0)
                {
                    // Load in same context
                    assembly = Assembly.LoadFrom(assemblyPath);
                }
                else
                {
                    AssemblyInstance assemblyInstance = AssemblyCache.Instance.Add(assemblyGuid, assemblyPath);
                    assembly = assemblyInstance.Assembly;
                }

                if (assembly == null)
                {
                    Logger.Log(LogType.Error, "Failed to load assembly: " + assemblyPath);

                    return (int)LoadAssemblyResult.NotFound;
                }

                AssemblyName hyperionCoreDependency = Array.Find(assembly.GetReferencedAssemblies(), (assemblyName) => assemblyName.Name == "HyperionCore");

                if (hyperionCoreDependency != null)
                {
                    string versionString = hyperionCoreDependency.Version.ToString();

                    var versionParts = versionString.Split('.');

                    uint majorVersion = versionParts.Length > 0 ? uint.Parse(versionParts[0]) : 0;
                    uint minorVersion = versionParts.Length > 1 ? uint.Parse(versionParts[1]) : 0;
                    uint patchVersion = versionParts.Length > 2 ? uint.Parse(versionParts[2]) : 0;

                    uint assemblyEngineVersion = (majorVersion << 16) | (minorVersion << 8) | patchVersion;

                    // Verify the engine version (major, minor)
                    if (!NativeInterop_VerifyEngineVersion(assemblyEngineVersion, true, true, false))
                    {
                        Logger.Log(LogType.Error, "Assembly version does not match engine version");

                        return (int)LoadAssemblyResult.VersionMismatch;
                    }
                }

                Type[] types = assembly.GetTypes();

                foreach (Type type in types)
                {
                    if (type.IsClass || (type.IsValueType && !type.IsEnum))
                    {
                        // Register classes and structs
                        InitManagedClass(assemblyGuid, classHolderPtr, type);
                    }
                }

                NativeInterop_SetInvokeMethodFunction(ref assemblyGuid, classHolderPtr, Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeMethod));
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

        [UnmanagedCallersOnly]
        public static unsafe void AddMethodToCache(IntPtr assemblyGuidPtr, IntPtr methodGuidPtr, IntPtr methodInfoPtr)
        {
            Guid assemblyGuid = Marshal.PtrToStructure<Guid>(assemblyGuidPtr);
            Guid methodGuid = Marshal.PtrToStructure<Guid>(methodGuidPtr);

            ref MethodInfo methodInfo = ref System.Runtime.CompilerServices.Unsafe.AsRef<MethodInfo>(methodInfoPtr.ToPointer());

            ManagedMethodCache.Instance.AddMethod(assemblyGuid, methodGuid, methodInfo);
        }

        [UnmanagedCallersOnly]
        public static unsafe void AddObjectToCache(IntPtr assemblyGuidPtr, IntPtr objectGuidPtr, IntPtr objectPtr, IntPtr outObjectReferencePtr, bool keepAlive)
        {
            Guid assemblyGuid = Marshal.PtrToStructure<Guid>(assemblyGuidPtr);
            Guid objectGuid = Marshal.PtrToStructure<Guid>(objectGuidPtr);

            // read object as reference
            ref object obj = ref System.Runtime.CompilerServices.Unsafe.AsRef<object>(objectPtr.ToPointer());

            ObjectReference objectReference = ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj, keepAlive);

            // write objectReference to outObjectReferencePtr
            Marshal.StructureToPtr(objectReference, outObjectReferencePtr, false);
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

        private static unsafe ManagedClass InitManagedClass(Guid assemblyGuid, IntPtr classHolderPtr, Type type)
        {
            ManagedClass managedClass = new ManagedClass();

            if (ManagedClass_FindByTypeHash(ref assemblyGuid, classHolderPtr, type.GetHashCode(), out managedClass))
            {
                return managedClass;
            }

            ManagedClass? parentClass = null;

            Type? baseType = type.BaseType;

            if (baseType != null)
            {
                parentClass = InitManagedClass(assemblyGuid, classHolderPtr, baseType);
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

            var classAttributes = type.GetCustomAttributes();

            foreach (var attr in classAttributes)
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

                    break;
                }
            }

            ManagedClass_Create(ref assemblyGuid, classHolderPtr, hypClassPtr, type.GetHashCode(), typeNamePtr, parentClass?.classObjectPtr ?? IntPtr.Zero, (uint)managedClassFlags, out managedClass);

            Marshal.FreeHGlobal(typeNamePtr);

            Dictionary<string, MethodInfo> methods = new Dictionary<string, MethodInfo>();
            CollectMethods(type, methods);

            foreach (var item in methods)
            {
                MethodInfo methodInfo = item.Value;
                
                // Get all custom attributes for the method
                object[] attributes = methodInfo.GetCustomAttributes(false /* inherit */);

                List<string> attributeNames = new List<string>();

                foreach (object attribute in attributes)
                {
                    // Add full qualified name of the attribute
                    attributeNames.Add(attribute.GetType().FullName);
                }

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                Guid methodGuid = Guid.NewGuid();
                managedClass.AddMethod(item.Key, methodGuid, attributeNames.ToArray());

                ManagedMethodCache.Instance.AddMethod(assemblyGuid, methodGuid, methodInfo);
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

                if (callbackPtr != IntPtr.Zero)
                {
                    if (!type.IsValueType)
                    {
                        throw new InvalidOperationException("InitializeObjectCallback can only be used with value types");
                    }

                    // If callback has been set, invoke it so we can set up the object from C++
                    GCHandle objectHandle = GCHandle.Alloc(obj, GCHandleType.Pinned);

                    InitializeObjectCallbackDelegate callbackDelegate = Marshal.GetDelegateForFunctionPointer<InitializeObjectCallbackDelegate>(callbackPtr);
                    callbackDelegate(contextPtr, objectHandle.AddrOfPinnedObject(), (uint)Marshal.SizeOf(type));

                    objectHandle.Free();
                }

                Guid objectGuid = Guid.NewGuid();
                
                return ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj, keepAlive);
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

                return ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj, false);
            });

            return managedClass;
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
            MethodInfo methodInfo = ManagedMethodCache.Instance.GetMethod(managedMethodGuid);

            Type returnType = methodInfo.ReturnType;
            Type thisType = methodInfo.DeclaringType;

            object[]? parameters = null;
            HandleParameters(paramsPtr, methodInfo, out parameters);

            object? thisObject = null;

            if (!methodInfo.IsStatic)
            {
                StoredManagedObject? storedObject = ManagedObjectCache.Instance.GetObject(thisObjectGuid);

                if (storedObject == null)
                {
                    throw new Exception("Failed to get target from GUID: " + thisObjectGuid);
                }

                thisObject = ((StoredManagedObject)storedObject).obj;
            }

            object? returnValue = methodInfo.Invoke(thisObject, parameters);

            MarshalHelpers.MarshalOutObject(outPtr, returnType, returnValue);
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
        private static extern bool ManagedClass_FindByTypeHash(ref Guid assemblyGuid, IntPtr classHolderPtr, int typeHash, [Out] out ManagedClass result);

        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "NativeInterop_VerifyEngineVersion")]
        private static extern bool NativeInterop_VerifyEngineVersion(uint assemblyEngineVersion, bool major, bool minor, bool patch);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetInvokeMethodFunction")]
        private static extern void NativeInterop_SetInvokeMethodFunction([In] ref Guid assemblyGuid, IntPtr classHolderPtr, IntPtr invokeMethodPtr);
    }
}
