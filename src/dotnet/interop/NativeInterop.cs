using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public delegate void InvokeMethodDelegate(IntPtr thisObjectReferencePtr, IntPtr argsHypDataPtr, IntPtr outReturnHypDataPtr);
    public delegate void InvokeGetterDelegate(Guid propertyGuid, IntPtr thisObjectReferencePtr, IntPtr argsHypDataPtr, IntPtr outReturnHypDataPtr);
    public delegate void InvokeSetterDelegate(Guid propertyGuid, IntPtr thisObjectReferencePtr, IntPtr argsHypDataPtr, IntPtr outReturnHypDataPtr);
    public delegate void InitializeObjectCallbackDelegate(IntPtr contextPtr, IntPtr objectPtr, uint objectSize);
    public delegate void AddObjectToCacheDelegate(IntPtr objectWrapperPtr, IntPtr outClassObjectPtr, IntPtr outObjectReferencePtr, bool weak);
    public delegate bool SetKeepAliveDelegate(IntPtr objectReferencePtr, bool keepAlive);
    public delegate void TriggerGCDelegate();
    public delegate bool GetAssemblyPointerDelegate(IntPtr assemblyObjectReferencePtr, IntPtr outAssemblyPtr);

    public enum LoadAssemblyResult : int
    {
        UnknownError = -100,
        VersionMismatch = -2,
        NotFound = -1,
        Ok = 0
    }

    public class NativeInterop
    {
        private const string ClassPtrFieldName = "_classPtr";
        private const string NativeAddressFieldName = "_nativeAddress";

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

        private static bool IsHyperionAssembly(Assembly assembly)
        {
            string assemblyName = assembly.GetName().Name ?? string.Empty;
            return assemblyName.StartsWith("Hyperion.");
        }

        private static void InitializeHyperionAssembly(Assembly assembly, bool isCoreAssembly)
        {
            if (!IsHyperionAssembly(assembly))
            {
                throw new InvalidOperationException("Assembly is not a Hyperion.* assembly: " + assembly.FullName);
            }

            Type[] types = assembly.GetExportedTypes();

            foreach (Type type in types)
            {
                if (type.IsGenericType)
                {
                    // skip generic types
                    continue;
                }

                if (type.IsClass || type.IsValueType || type.IsEnum)
                {
                    InitManagedClass(type, isCoreAssembly);
                }
            }
        }

        [UnmanagedCallersOnly]
        public static unsafe int InitializeRuntime()
        {
            return InitializeRuntimeManaged();
        }

        public static unsafe int InitializeRuntimeManaged()
        {
            try
            {
                AppDomain currentDomain = AppDomain.CurrentDomain;

                Logger.Log(LogType.Info, "Initializing .NET runtime in AppDomain: {0}, ID: {1}", currentDomain.FriendlyName, currentDomain.Id);

                currentDomain.UnhandledException += new UnhandledExceptionEventHandler(HandleUnhandledException);

                NativeInterop_SetAddObjectToCacheFunction(Marshal.GetFunctionPointerForDelegate<AddObjectToCacheDelegate>(AddObjectToCache));
                NativeInterop_SetSetKeepAliveFunction((delegate* unmanaged<IntPtr, int*, void>)&SetKeepAlive);
                NativeInterop_SetTriggerGCFunction((delegate* unmanaged<void>)&TriggerGC);
                NativeInterop_SetGetAssemblyPointerFunction((delegate* unmanaged<IntPtr, IntPtr, void>)&GetAssemblyPointer);
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, "Error loading assembly: {0}", ex);

                return (int)LoadAssemblyResult.UnknownError;
            }

            return (int)LoadAssemblyResult.Ok;
        }

        [UnmanagedCallersOnly]
        public static unsafe int InitializeAssembly(IntPtr outAssemblyGuid, IntPtr assemblyPtr, IntPtr assemblyPathStringPtr, int isCoreAssembly)
        {
            return InitializeAssemblyManaged(outAssemblyGuid, assemblyPtr, assemblyPathStringPtr, isCoreAssembly);
        }

        public static unsafe int InitializeAssemblyManaged(IntPtr outAssemblyGuid, IntPtr assemblyPtr, IntPtr assemblyPathStringPtr, int isCoreAssembly)
        {
            try
            {
                // Create a managed string from the pointer
                string assemblyPath = Marshal.PtrToStringAnsi(assemblyPathStringPtr) ?? string.Empty;

                Logger.Log(LogType.Debug, "Initializing assembly {0}, is core assembly? {1}", assemblyPath, isCoreAssembly);

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

                bool ownsAssemblyPtr = assemblyPtr == IntPtr.Zero;
                if (ownsAssemblyPtr)
                {
                    int res = NativeInterop_NewAssembly(assemblyGuid, out assemblyPtr);
                    if (res != (int)LoadAssemblyResult.Ok)
                    {
                        Logger.Log(LogType.Error, "Failed to allocate new assembly at " + assemblyPath + ". Error code: " + (LoadAssemblyResult)res);

                        return res;
                    }
                }

                AssemblyInstance assemblyInstance = AssemblyCache.Instance.Add(
                    guid: assemblyGuid,
                    path: assemblyPath,
                    assemblyPtr: assemblyPtr,
                    ownsAssemblyPtr: ownsAssemblyPtr,
                    isCoreAssembly: isCoreAssembly != 0);

                Assembly? assembly = assemblyInstance.Assembly;

                if (assembly == null)
                {
                    Logger.Log(LogType.Error, "Failed to load assembly: " + assemblyPath);

                    return (int)LoadAssemblyResult.NotFound;
                }

                // check if it has a dependency on the engine - if so, we need to verify the version is compatible
                AssemblyName? hyperionSharedDependency = Array.Find(assembly.GetReferencedAssemblies(), (assemblyName) => assemblyName.Name == "Hyperion.NET.Shared");
                if (hyperionSharedDependency != null)
                {
                    // Verify the engine version (major, minor)
                    if (!VerifyEngineVersion(hyperionSharedDependency.Version?.ToString() ?? string.Empty, true, true, false))
                    {
                        Logger.Log(LogType.Error, "Assembly version does not match engine version");

                        return (int)LoadAssemblyResult.VersionMismatch;
                    }
                }

                if (assemblyPtr != IntPtr.Zero)
                {
                    NativeInterop_SetInvokeGetterFunction(ref assemblyGuid, assemblyPtr, Marshal.GetFunctionPointerForDelegate<InvokeGetterDelegate>(InvokeGetter));
                    NativeInterop_SetInvokeSetterFunction(ref assemblyGuid, assemblyPtr, Marshal.GetFunctionPointerForDelegate<InvokeSetterDelegate>(InvokeSetter));
                }

                if (IsHyperionAssembly(assembly))
                {
                    InitializeHyperionAssembly(assembly, isCoreAssembly != 0);
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, "Error loading assembly: {0}", ex);

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

                    foreach (var kv in AssemblyCache.Instance.GetAssemblies())
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
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, "Error unloading assembly: {0}", ex);

                result = false;
            }

            Marshal.WriteInt32(outResult, result ? 1 : 0);
        }

        private static unsafe ManagedAttributeHolder AllocAttributeHolder(Guid assemblyGuid, IntPtr assemblyPtr, object[] attributes)
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
                Assert.Throw(attribute != null);

                Type attributeType = attribute!.GetType();

                IntPtr pClass = InitManagedClass(attributeType, isCoreAssembly: false);

                ObjectReference attributeObjectReference = new ObjectReference
                {
                    WeakHandle = GCHandle.ToIntPtr(GCHandle.Alloc(attribute, GCHandleType.Weak)),
                    StrongHandle = IntPtr.Zero
                };

                ref ManagedAttribute managedAttribute = ref Unsafe.AsRef<ManagedAttribute>((void*)(managedAttributeHolder.managedAttributesPtr + (i * Marshal.SizeOf<ManagedAttribute>())));
                managedAttribute.pClass = pClass;
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

        private static object? TryGetAttributeByName(Type type, string name)
        {
            foreach (var attr in type.GetCustomAttributes(false))
            {
                if (attr.GetType().Name == name)
                {
                    return attr;
                }
            }

            return null;
        }

        private static unsafe IntPtr InitManagedClass(Type type, bool isCoreAssembly)
        {
            // Skip classes with the NoManagedClass attribute
            if (TryGetAttributeByName(type, "NoManagedClass") != null)
            {
                Logger.Log(LogType.Debug, "Skipping managed class for type: {0} due to NoManagedClass attribute", type.Name);

                return IntPtr.Zero;
            }

            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.Get(type.Assembly);

            if (assemblyInstance == null)
            {
                throw new Exception("Failed to get assembly instance for type: " + type.Name + " from assembly: " + type.Assembly.FullName
                    + " located at: " + type.Assembly.Location
                    + ", has the assembly been registered?");
            }

            Guid assemblyGuid = assemblyInstance.Guid;
            IntPtr assemblyPtr = assemblyInstance.AssemblyPtr;

            if (assemblyPtr == IntPtr.Zero)
            {
                throw new Exception("Assembly pointer is null for assembly: " + type.Assembly.FullName + " located at: " + type.Assembly.Location);
            }

            IntPtr foundClassObjectPtr = IntPtr.Zero;

            // Check if class has already been initialized
            if (ManagedClass_FindByTypeHash(assemblyPtr, type.GetHashCode(), out foundClassObjectPtr))
            {
                return foundClassObjectPtr;
            }

            IntPtr parentClassObjectPtr = IntPtr.Zero;

            Type? baseType = type.BaseType;

            if (baseType != null)
            {
                parentClassObjectPtr = InitManagedClass(baseType, isCoreAssembly);
            }

            // Check if initializing parent class has caused this class to be initialized
            if (ManagedClass_FindByTypeHash(assemblyPtr, type.GetHashCode(), out foundClassObjectPtr))
            {
                return foundClassObjectPtr;
            }

            ManagedClassDesc managedClassDesc = new ManagedClassDesc();

            string typeName = type.Name;
            IntPtr typeNamePtr = Marshal.StringToHGlobalAnsi(typeName);

            string? className = null;
            IntPtr classPtr = IntPtr.Zero;

            TypeId typeId = TypeId.ForType(type);

            // Use dynamic since we don't know the actual type - it is loaded from another assembly
            dynamic? classBindingAttribute = TryGetAttributeByName(type, "ClassBinding");

            if (classBindingAttribute != null)
            {
                className = classBindingAttribute.Name;

                if (className == null || className.Length == 0)
                    className = typeName;

                bool isDynamic = classBindingAttribute.IsDynamic;

                if (isDynamic)
                {
                    // We only register dynamic classes in core assemblies, otherwise they would be constantly invalidated on assembly reload.
                    // No need to use GetClass() in C++ to fetch non-core assembly classes, as it would be totally context dependent, anyway.
                    if (isCoreAssembly)
                    {
                        // Find closest parent class with ClassBinding attribute
                        Type? parentType = type.BaseType;
                        IntPtr parentClassPtr = IntPtr.Zero;

                        while (parentType != null)
                        {
                            dynamic? parentClassBindingAttribute = TryGetAttributeByName(parentType, "ClassBinding");

                            if (parentClassBindingAttribute != null)
                            {
                                // Call the GetClass method
                                dynamic? parentClass = parentClassBindingAttribute.GetClass(parentType);
                                parentClassPtr = parentClass?.Address ?? IntPtr.Zero;

                                if (parentClassPtr != IntPtr.Zero)
                                    break;
                            }

                            parentType = parentType.BaseType;
                        }

                        if (parentClassPtr == IntPtr.Zero)
                            throw new Exception(string.Format("To create a dynamic Class, a parent class must exist with a valid ClassBinding attribute!"));

                        // @FIXME: Allocated but never deleted. Need to implement deletion and removal from global array on assembly unload.
                        classPtr = Class_CreateDynamicClass(ref typeId, className, parentClassPtr);

                        if (classPtr == IntPtr.Zero)
                            throw new Exception(string.Format("Failed to create a dynamic Class for type \"{0}\" (TypeId: {1})", type.Name, typeId));
                    }
                }
                else
                {
                    classPtr = Class_GetClassByName(className);

                    if (classPtr == IntPtr.Zero)
                        throw new Exception(string.Format("No Class found for \"{0}\"", className));
                }
            }

            uint typeSize = 0;

            ManagedClassFlags managedClassFlags = ManagedClassFlags.None;

            if (type.IsClass)
            {
                managedClassFlags |= ManagedClassFlags.ClassType;
            }
            else if (type.IsValueType && !type.IsEnum)
            {
                managedClassFlags |= ManagedClassFlags.StructType;

                if (!type.IsGenericType)
                {
                    typeSize = (uint)Marshal.SizeOf(type);
                }
            }
            else if (type.IsEnum)
            {
                managedClassFlags |= ManagedClassFlags.EnumType;
                typeSize = (uint)Marshal.SizeOf(Enum.GetUnderlyingType(type));
            }

            if (type.IsAbstract)
            {
                managedClassFlags |= ManagedClassFlags.Abstract;
            }

            ManagedClass_Create(ref assemblyGuid, assemblyPtr, classPtr, type.GetHashCode(), typeNamePtr, typeSize, typeId, parentClassObjectPtr, (uint)managedClassFlags, out managedClassDesc);

            Marshal.FreeHGlobal(typeNamePtr);

            ManagedAttributeHolder managedAttributeHolder = AllocAttributeHolder(assemblyGuid, assemblyPtr, type.GetCustomAttributes().ToArray());
            managedClassDesc.SetAttributes(ref managedAttributeHolder);
            managedAttributeHolder.Dispose();

            foreach (var item in CollectMethods(type))
            {
                MethodInfo methodInfo = item.Value;

                managedAttributeHolder = AllocAttributeHolder(assemblyGuid, assemblyPtr, methodInfo.GetCustomAttributes(false));

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                Guid methodGuid = Guid.NewGuid();

                InvokeMethodDelegate invokeMethodDelegate = (IntPtr thisObjectReferencePtr, IntPtr argsPtr, IntPtr retPtr) =>
                {
                    try
                    {
#if DEBUG
                        Assert.Throw(methodInfo != null, "MethodInfo is null for method: " + item.Key + " in type: " + type.Name);
#endif

                        object?[] parameters;
                        HandleParameters(argsPtr, methodInfo, out parameters);

                        object? thisObject = null;

                        if (!methodInfo.IsStatic)
                        {
#if DEBUG
                            Assert.Throw(thisObjectReferencePtr != IntPtr.Zero, "This object reference pointer is null for method: " + methodInfo.Name + " in type: " + type.Name);
#endif

                            ref ObjectReference objectReferenceRef = ref Unsafe.AsRef<ObjectReference>((void*)thisObjectReferencePtr);

                            thisObject = objectReferenceRef.LoadObject();

                            if (thisObject == null)
                                throw new InvalidOperationException("Failed to get object reference for method: " + methodInfo.Name + " from " + methodInfo.DeclaringType?.Name);
                        }

                        if (methodInfo.ReturnType == typeof(void))
                        {
                            methodInfo.Invoke(thisObject, parameters);
                            return;
                        }

                        object? returnValue = methodInfo.Invoke(thisObject, parameters);

                        if (retPtr != IntPtr.Zero)
                        {
                            ((HypDataBuffer*)retPtr)->SetValue(returnValue);
                        }
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogType.Error, "Error invoking method {0} on type {1}: {2}", methodInfo.Name, methodInfo.DeclaringType?.Name, ex);

                        throw;
                    }
                };

                IntPtr functionPointer = Marshal.GetFunctionPointerForDelegate(invokeMethodDelegate);
                ManagedMethodCache.Instance.Add(assemblyGuid, methodGuid, methodInfo, invokeMethodDelegate);

                managedClassDesc.AddMethod(item.Key, methodGuid, functionPointer, ref managedAttributeHolder);

                managedAttributeHolder.Dispose();
            }

            foreach (var item in CollectProperties(type))
            {
                PropertyInfo propertyInfo = item.Value;

                managedAttributeHolder = AllocAttributeHolder(assemblyGuid, assemblyPtr, propertyInfo.GetCustomAttributes(false));

                Guid propertyGuid = Guid.NewGuid();
                managedClassDesc.AddProperty(item.Key, propertyGuid, ref managedAttributeHolder);

                BasicCache<PropertyInfo>.Instance.Add(assemblyGuid, propertyGuid, propertyInfo);

                managedAttributeHolder.Dispose();
            }

            // Add new object, free object delegates
            managedClassDesc.SetNewObjectFunction(assemblyGuid, new NewObjectDelegate((bool keepAlive, IntPtr classPtr, IntPtr nativeAddress, IntPtr contextPtr, IntPtr callbackPtr) =>
            {
                // Allocate the object
                object obj = RuntimeHelpers.GetUninitializedObject(type);
                Assert.Throw(obj != null);

                // Call the constructor
                ConstructorInfo? constructorInfo;
                object[]? parameters = null;

                if (classPtr != IntPtr.Zero)
                {
                    if (nativeAddress == IntPtr.Zero)
                        throw new ArgumentNullException(nameof(nativeAddress));

                    Type? objType = obj!.GetType();

                    if (objType == null)
                        throw new InvalidOperationException("Failed to get object type for object of type: " + type.Name);

                    FieldInfo? classPtrField = objType.GetField(ClassPtrFieldName, BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.FlattenHierarchy);
                    FieldInfo? nativeAddressField = objType.GetField(NativeAddressFieldName, BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.FlattenHierarchy);

                    if (classPtrField == null || nativeAddressField == null)
                        throw new InvalidOperationException("Could not find classPtr or nativeAddress field on class " + type.Name);

                    classPtrField.SetValue(obj, classPtr);
                    nativeAddressField.SetValue(obj, nativeAddress);
                }

                constructorInfo = type.GetConstructor(BindingFlags.Public | BindingFlags.Instance, null, Type.EmptyTypes, null);

                if (constructorInfo == null)
                    throw new InvalidOperationException("Failed to find empty constructor for type: " + type.Name);

                constructorInfo.Invoke(obj, parameters);

                GCHandle gcHandleWeak = GCHandle.Alloc(obj, GCHandleType.Weak);
                GCHandle? gcHandleStrong = null;

                if (callbackPtr != IntPtr.Zero)
                {
                    if (!type.IsValueType)
                        throw new InvalidOperationException("InitializeObjectCallback can only be used with value types");

                    gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Pinned);

                    InitializeObjectCallbackDelegate callbackDelegate = Marshal.GetDelegateForFunctionPointer<InitializeObjectCallbackDelegate>(callbackPtr);
                    callbackDelegate(contextPtr, ((GCHandle)gcHandleStrong).AddrOfPinnedObject(), (uint)Marshal.SizeOf(type));

                    if (!keepAlive)
                    {
                        ((GCHandle)gcHandleStrong).Free();
                        gcHandleStrong = null;
                    }
                }
                else if (keepAlive)
                {
                    gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);
                }

                return new ObjectReference
                {
                    WeakHandle = GCHandle.ToIntPtr(gcHandleWeak),
                    StrongHandle = gcHandleStrong.HasValue ? GCHandle.ToIntPtr((GCHandle)gcHandleStrong) : IntPtr.Zero
                };
            }));

            managedClassDesc.SetMarshalObjectFunction(assemblyGuid, new MarshalObjectDelegate((IntPtr ptr, uint size) =>
            {
#if DEBUG
                if (ptr == IntPtr.Zero)
                    throw new ArgumentNullException(nameof(ptr));

                if (size != Marshal.SizeOf(type))
                    throw new ArgumentException("Size does not match type size", nameof(size));
#endif

                // Marshal object from pointer
                object? obj = Marshal.PtrToStructure(ptr, type);
                Assert.Throw(obj != null, "Failed to marshal object from pointer");

                return new ObjectReference
                {
                    WeakHandle = GCHandle.ToIntPtr(GCHandle.Alloc(obj, GCHandleType.Weak)),
                    StrongHandle = IntPtr.Zero
                };
            }));

            return managedClassDesc.ClassObjectPtr;
        }

        private static unsafe void HandleParameters(IntPtr argsHypDataPtr, MethodInfo methodInfo, out object?[] parameters)
        {
            int numParams = methodInfo.GetParameters().Length;

            if (numParams == 0)
            {
                parameters = Array.Empty<object?>();

                return;
            }

            parameters = new object?[numParams];

            HypDataBuffer* paramPtr = *(HypDataBuffer**)argsHypDataPtr;
            int paramsOffset = 0;

            for (int paramIndex = 0; paramIndex < numParams; paramIndex++)
            {
                // Get the ParameterInfo for the current parameter
                ParameterInfo parameterInfo = methodInfo.GetParameters()[paramIndex];

                Type parameterType = parameterInfo.ParameterType;

                // Check if it has ParamArrayAttribute (params)
                if (parameterInfo.GetCustomAttribute(typeof(ParamArrayAttribute)) != null)
                {
                    // Calculate array size by iterating until we hit a null pointer.
                    int paramArraySize = 0;

                    for (IntPtr currentParamPtr = argsHypDataPtr + paramsOffset; (IntPtr)(*((HypDataBuffer**)currentParamPtr)) != IntPtr.Zero; currentParamPtr += sizeof(IntPtr))
                    {
                        paramArraySize++;
                    }

                    // Empty array
                    if (paramArraySize == 0)
                    {
                        parameters[paramIndex] = Array.CreateInstance(parameterType.GetElementType()!, 0);

                        break;
                    }

                    // We need to read the params array from the argsHypDataPtr

                    Array paramArray = Array.CreateInstance(parameterType.GetElementType()!, paramArraySize);

                    // Copy the values from the list to the array
                    int paramElementIndex = 0;

                    while ((IntPtr)paramPtr != IntPtr.Zero)
                    {
                        object? paramValue;

                        try
                        {
                            paramValue = paramPtr->GetValue();
                        }
                        catch (Exception ex)
                        {
                            throw new Exception("Failed to get params element value at index: " + paramElementIndex + " for method: " + methodInfo.Name + " from " + methodInfo.DeclaringType?.Name, ex);
                        }

                        paramArray.SetValue(paramValue, paramElementIndex);

                        paramsOffset += sizeof(IntPtr);
                        paramPtr = *(HypDataBuffer**)(argsHypDataPtr + paramsOffset);

                        paramElementIndex++;
                    }

                    parameters[paramIndex] = paramArray;

                    break;
                }

                try
                {
                    parameters[paramIndex] = paramPtr->GetValue();
                }
                catch (Exception ex)
                {
                    throw new Exception("Failed to get parameter value at index: " + paramIndex + " for method: " + methodInfo.Name + " from " + methodInfo.DeclaringType?.Name, ex);
                }

                paramsOffset += sizeof(IntPtr);
                paramPtr = *(HypDataBuffer**)(argsHypDataPtr + paramsOffset);
            }
        }

        public static unsafe void InvokeGetter(Guid managedPropertyGuid, IntPtr thisObjectReferencePtr, IntPtr argsHypDataPtr, IntPtr outReturnHypDataPtr)
        {
            PropertyInfo propertyInfo = BasicCache<PropertyInfo>.Instance.Get(managedPropertyGuid);

            ref ObjectReference objectReferenceRef = ref Unsafe.AsRef<ObjectReference>((void*)thisObjectReferencePtr);

            object? thisObject = objectReferenceRef.LoadObject();
            object? returnValue = propertyInfo.GetValue((object?)thisObject);

            ((HypDataBuffer*)outReturnHypDataPtr)->SetValue(returnValue);
        }

        public static unsafe void InvokeSetter(Guid managedPropertyGuid, IntPtr thisObjectReferencePtr, IntPtr argsHypDataPtr, IntPtr outReturnHypDataPtr)
        {
            PropertyInfo propertyInfo = BasicCache<PropertyInfo>.Instance.Get(managedPropertyGuid);

            ref ObjectReference objectReferenceRef = ref Unsafe.AsRef<ObjectReference>((void*)thisObjectReferencePtr);

            object? thisObject = objectReferenceRef.LoadObject();
            object? value = (*(HypDataBuffer**)argsHypDataPtr)->GetValue();

            propertyInfo.SetValue((object?)thisObject, value);
        }

        public static unsafe void AddObjectToCache(IntPtr objectWrapperPtr, IntPtr outClassObjectPtr, IntPtr outObjectReferencePtr, bool weak)
        {
            ref ObjectWrapper objectWrapperRef = ref Unsafe.AsRef<ObjectWrapper>((void*)objectWrapperPtr);
            ref ObjectReference objectReferenceRef = ref Unsafe.AsRef<ObjectReference>((void*)outObjectReferencePtr);

            object obj = objectWrapperRef.obj;

            if (obj == null)
                throw new ArgumentNullException(nameof(obj));

            Type type = obj.GetType();

            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.Get(type.Assembly);

            if (assemblyInstance == null)
            {
                throw new Exception("Failed to get assembly instance for type: " + type.Name + " from assembly: " + type.Assembly.FullName + ", has the assembly been registered?");
            }

            Guid assemblyGuid = assemblyInstance.Guid;
            IntPtr assemblyPtr = assemblyInstance.AssemblyPtr;

            if (assemblyPtr == IntPtr.Zero)
            {
                throw new Exception("Assembly pointer is null for assembly: " + type.Assembly.FullName + ", has the assembly been registered?");
            }

            // ManagedClass must be registered for the given object's type.
            IntPtr pClass;
            if (!ManagedClass_FindByTypeHash(assemblyPtr, type.GetHashCode(), out pClass))
            {
                throw new Exception("ManagedClass not found for Type " + type.Name + " from assembly: " + type.Assembly.FullName + ", has the assembly been registered? Ensure the class or struct is public.");
            }

            Marshal.WriteIntPtr(outClassObjectPtr, pClass);

            GCHandle gcHandleWeak = GCHandle.Alloc(obj, GCHandleType.Weak);
            GCHandle? gcHandleStrong = null;

            if (!weak)
                gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);

#if DEBUG
            Assert.Throw(objectReferenceRef.WeakHandle == IntPtr.Zero && objectReferenceRef.StrongHandle == IntPtr.Zero, "ObjectReference already has handles assigned");
#endif

            // @NOTE: reassign ref
            objectReferenceRef = new ObjectReference
            {
                WeakHandle = GCHandle.ToIntPtr(gcHandleWeak),
                StrongHandle = gcHandleStrong.HasValue ? GCHandle.ToIntPtr(gcHandleStrong.Value) : IntPtr.Zero
            };
        }

        [UnmanagedCallersOnly]
        public static unsafe void SetKeepAlive(IntPtr objectReferencePtr, int* inOutKeepAlive)
        {
            ref ObjectReference objectReference = ref Unsafe.AsRef<ObjectReference>((void*)objectReferencePtr);

            if (*inOutKeepAlive != 0)
            {
                if (objectReference.StrongHandle != IntPtr.Zero)
                {
                    // Already allocated
                    *inOutKeepAlive = 1;
                    return;
                }

                if (objectReference.WeakHandle == IntPtr.Zero)
                {
                    *inOutKeepAlive = 0;
                    return;
                }

                object? obj = objectReference.LoadObject();

                if (obj == null)
                {
                    *inOutKeepAlive = 0;
                    return;
                }

                objectReference.StrongHandle = GCHandle.ToIntPtr(GCHandle.Alloc(obj, GCHandleType.Normal));

                *inOutKeepAlive = 1;

                return;
            }

            // No weak reference
            if (objectReference.WeakHandle == IntPtr.Zero)
            {
                *inOutKeepAlive = 0;
                return;
            }

            // Free the strong handle
            if (objectReference.StrongHandle != IntPtr.Zero)
            {
                GCHandle.FromIntPtr(objectReference.StrongHandle).Free();
                objectReference.StrongHandle = IntPtr.Zero;
            }

            *inOutKeepAlive = 1;

            return;
        }

        [UnmanagedCallersOnly]
        public static unsafe void TriggerGC()
        {
            GC.Collect();
        }

        [UnmanagedCallersOnly]
        public static unsafe void GetAssemblyPointer(IntPtr assemblyObjectReferencePtr, IntPtr outAssemblyPtr)
        {
            Marshal.WriteIntPtr(outAssemblyPtr, IntPtr.Zero);

            ref ObjectReference objectReference = ref Unsafe.AsRef<ObjectReference>((void*)assemblyObjectReferencePtr);

            Assembly? assembly = (Assembly?)objectReference.LoadObject();

            if (assembly == null)
                return;

            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.Get(assembly);

            if (assemblyInstance == null)
                return;

            Marshal.WriteIntPtr(outAssemblyPtr, assemblyInstance.AssemblyPtr);
        }

        public static void HandleUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            Logger.Log(LogType.Error, "Unhandled managed exception: {0}\n\n{1}", ((Exception)e.ExceptionObject).Message, ((Exception)e.ExceptionObject).StackTrace);

            MessageBox.Critical()
                .Title("Uncaught Exception")
                .Text("An unhandled exception occurred in managed code!\n"
                    + ((Exception)e.ExceptionObject).Message + "\n\n"
                    + "Check the logs for more details.")
                .Button("OK", () => { })
                .Show();
        }

        [DllImport("hyperion", EntryPoint = "ManagedClass_Create")]
        private static extern void ManagedClass_Create(ref Guid assemblyGuid, IntPtr assemblyPtr, IntPtr classPtr, int typeHash, IntPtr typeNamePtr, uint typeSize, TypeId typeId, IntPtr parentClassPtr, uint managedClassFlags, [Out] out ManagedClassDesc outDesc);

        [DllImport("hyperion", EntryPoint = "ManagedClass_FindByTypeHash")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ManagedClass_FindByTypeHash([In] IntPtr assemblyPtr, int typeHash, [Out] out IntPtr outManagedClassObjectPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetClassByName")]
        private static extern IntPtr Class_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "Class_GetClassByTypeId")]
        private static extern IntPtr Class_GetClassByTypeId([In] ref TypeId typeId);

        [DllImport("hyperion", EntryPoint = "Class_GetClassForManagedClass")]
        private static extern IntPtr Class_GetClassForManagedClass([In] IntPtr pClass);

        [DllImport("hyperion", EntryPoint = "Class_CreateDynamicClass")]
        private static extern IntPtr Class_CreateDynamicClass([In] ref TypeId typeId, [MarshalAs(UnmanagedType.LPStr)] string name, [In] IntPtr parentClassClassPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_VerifyEngineVersion")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool NativeInterop_VerifyEngineVersion(uint assemblyEngineVersion, bool major, bool minor, bool patch);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetInvokeGetterFunction")]
        private static extern void NativeInterop_SetInvokeGetterFunction([In] ref Guid assemblyGuid, IntPtr assemblyPtr, IntPtr invokeGetterPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetInvokeSetterFunction")]
        private static extern void NativeInterop_SetInvokeSetterFunction([In] ref Guid assemblyGuid, IntPtr assemblyPtr, IntPtr invokeSetterPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetAddObjectToCacheFunction")]
        private static extern void NativeInterop_SetAddObjectToCacheFunction(IntPtr addObjectToCachePtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetSetKeepAliveFunction")]
        private static extern unsafe void NativeInterop_SetSetKeepAliveFunction(void* setKeepAlivePtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetTriggerGCFunction")]
        private static extern unsafe void NativeInterop_SetTriggerGCFunction(void* setTriggerGCFunctionPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetGetAssemblyPointerFunction")]
        private static extern unsafe void NativeInterop_SetGetAssemblyPointerFunction(void* getAssemblyPointerFunctionPtr);

        [DllImport("hyperion")]
        private static extern int NativeInterop_NewAssembly(Guid guid, out IntPtr outAssemblyPtr);

        [DllImport("hyperion")]
        private static extern void NativeInterop_FreeAssembly(IntPtr assemblyPtr);
    }
}
