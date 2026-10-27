using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Runtime.CompilerServices;
using System.Diagnostics;

namespace Hyperion
{
    public delegate void AddObjectToCacheDelegate(IntPtr objectWrapperPtr, IntPtr outClassObjectPtr, IntPtr outObjectReferencePtr, [MarshalAs(UnmanagedType.I1)] bool weak);
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
        private static readonly ConcurrentDictionary<IntPtr, int> s_liveObjectCounts = new ConcurrentDictionary<IntPtr, int>();

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

            return assemblyName.StartsWith("Hyperion.") || HypTypeRegistry.HasRegistry(assembly);
        }

        private static void InitializeHyperionAssembly(Assembly assembly, bool isCoreAssembly)
        {
            Logger.Log(LogLevel.Verbose, "Initializing Hyperion assembly: {0}", assembly.FullName);

            if (!IsHyperionAssembly(assembly))
            {
                throw new InvalidOperationException("Assembly is not a Hyperion.* assembly: " + assembly.FullName);
            }

            if (!HypTypeRegistry.HasRegistry(assembly))
            {
                Logger.Log(LogLevel.Error, "{0} has no type registry: build it with Hyperion.NET.Generators as an analyzer", assembly.GetName().Name);
            }

            if ((assembly.GetName().Name ?? string.Empty).StartsWith(EngineAssemblies.PluginPrefix, StringComparison.Ordinal))
            {
                try
                {
                    NativeLibrary.SetDllImportResolver(assembly, ResolvePluginLibrary);
                }
                catch (InvalidOperationException)
                {
                    // set already
                }
            }

            IEnumerable<Type> types = HypTypeRegistry.GetTypes(assembly);

            s_registrationStats = default;

            Stopwatch registrationTimer = Stopwatch.StartNew();

            foreach (Type type in types)
            {
                if (type.IsGenericType)
                {
                    // skip generic types
                    continue;
                }

                Logger.Log(LogLevel.Verbose, "Processing type: {0} for assembly: {1}", type.FullName, assembly.FullName);

                if (type.IsClass || type.IsValueType || type.IsEnum)
                {
                    InitManagedClass(type, isCoreAssembly);
                }

                if (!isCoreAssembly && type.IsValueType && !type.IsEnum)
                {
                    RegisterComponentType(type);
                }
            }

            Logger.Log(LogLevel.Debug, "Registered {0} managed class(es) from {1} in {5} ms: {2} methods, {3} properties, {4} constructors",
                s_registrationStats.Classes,
                assembly.GetName().Name,
                s_registrationStats.Methods,
                s_registrationStats.Properties,
                s_registrationStats.Constructors,
                registrationTimer.ElapsedMilliseconds);
        }

        // a plugin assembly's bindings import from hyperion-plugin-<Module>, which the engine loaded from its Plugins folder, or which is
        // linked into the engine library when plugins are
        private static IntPtr ResolvePluginLibrary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            if (!libraryName.StartsWith("hyperion-plugin-", StringComparison.Ordinal))
            {
                return IntPtr.Zero;
            }

            if (NativeLibrary.TryLoad(libraryName, assembly, searchPath, out IntPtr handle))
            {
                return handle;
            }

            string fileName;

            if (OperatingSystem.IsWindows())
            {
                fileName = libraryName + ".dll";
            }
            else if (OperatingSystem.IsMacOS())
            {
                fileName = "lib" + libraryName + ".dylib";
            }
            else
            {
                fileName = "lib" + libraryName + ".so";
            }
            string pluginsDirectory = System.IO.Path.Combine(AppContext.BaseDirectory, "Plugins");

            if (System.IO.Directory.Exists(pluginsDirectory))
            {
                foreach (string pluginDirectory in System.IO.Directory.GetDirectories(pluginsDirectory))
                {
                    string libraryPath = System.IO.Path.Combine(pluginDirectory, fileName);

                    if (System.IO.File.Exists(libraryPath) && NativeLibrary.TryLoad(libraryPath, out handle))
                    {
                        return handle;
                    }
                }
            }

            if (NativeLibrary.TryLoad("hyperion", assembly, searchPath, out handle))
            {
                return handle;
            }

            return IntPtr.Zero;
        }

        private static void RegisterComponentType(Type type)
        {
            try
            {
                ComponentRegistry.RegisterDeclaredComponent(type);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Failed to register component type {0}: {1}", type.FullName, ex);
            }
        }

        public static int InitializeLoadedAssembly(Assembly assembly, bool isCoreAssembly)
        {
            try
            {
                AssemblyCache.Instance.GetOrAdd(assembly, isCoreAssembly);

                if (IsHyperionAssembly(assembly))
                {
                    InitializeHyperionAssembly(assembly, isCoreAssembly);
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Error initializing assembly {0}: {1}", assembly.GetName().Name, ex);

                return (int)LoadAssemblyResult.UnknownError;
            }

            return (int)LoadAssemblyResult.Ok;
        }

        // Script components resolve their classes in this (already loaded) assembly instead of loading a script assembly
        public static void SetHostedScriptAssembly(Assembly assembly)
        {
            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.GetByReference(assembly);

            NativeInterop_SetHostedScriptAssembly(assemblyInstance != null ? assemblyInstance.AssemblyPtr : IntPtr.Zero);
        }

        public static unsafe int InitializeRuntimeManaged()
        {
            try
            {
                AppDomain currentDomain = AppDomain.CurrentDomain;

                currentDomain.UnhandledException += new UnhandledExceptionEventHandler(HandleUnhandledException);

                NativeInterop_SetAddObjectToCacheFunction((IntPtr)(delegate* unmanaged<IntPtr, IntPtr, IntPtr, byte, void>)&AddObjectToCacheUnmanaged);
                NativeInterop_SetSetKeepAliveFunction((delegate* unmanaged<IntPtr, int*, void>)&SetKeepAlive);
                NativeInterop_SetTriggerGCFunction((delegate* unmanaged<void>)&TriggerGC);
                NativeInterop_SetCollectGarbageAndWaitFunction((delegate* unmanaged<void>)&CollectGarbageAndWait);
                NativeInterop_SetGetAssemblyPointerFunction((delegate* unmanaged<IntPtr, IntPtr, void>)&GetAssemblyPointer);
                NativeInterop_SetCleanupOnShutdownFunction((delegate* unmanaged<void>)&CleanupOnShutdown);
                NativeInterop_SetRemoveObjectFromCacheFunction((delegate* unmanaged<IntPtr, void>)&RemoveObjectFromCache);
                NativeInterop_SetFreeObjectReferenceFunction((delegate* unmanaged<IntPtr, void>)&FreeObjectReference);
                NativeInterop_SetQueryManagedObjectCountsFunction((delegate* unmanaged<IntPtr*, int, int>)&QueryManagedObjectCounts);
                NativeInterop_SetGetTotalMemoryFunction((delegate* unmanaged<long>)&GetTotalMemory);
                NativeInterop_SetSimTickFunction((delegate* unmanaged<float, void>)&SimTick);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Error loading assembly: {0}", ex);

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
            return InitializeAssemblyManaged(outAssemblyGuid, assemblyPtr, assemblyPathStringPtr, isCoreAssembly, out _);
        }

        // error is the exception behind an UnknownError result, so a managed caller can report it
        public static unsafe int InitializeAssemblyManaged(IntPtr outAssemblyGuid, IntPtr assemblyPtr, IntPtr assemblyPathStringPtr, int isCoreAssembly, out Exception? error)
        {
            error = null;

            try
            {
                // Create a managed string from the pointer
                string assemblyPath = Marshal.PtrToStringUTF8(assemblyPathStringPtr) ?? string.Empty;

                if (isCoreAssembly != 0)
                {
                    // Check for assemblies having already been loaded
                    if (AssemblyCache.Instance.Get(assemblyPath) != null)
                    {
                        // throw new Exception("Assembly already loaded: " + assemblyPath + " (" + AssemblyCache.Instance.Get(assemblyPath).Guid + ")");

                        Logger.Log(LogLevel.Info, "Assembly already loaded: {0}", assemblyPath);

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
                        Logger.Log(LogLevel.Error, "Failed to allocate new assembly at " + assemblyPath + ". Error code: " + (LoadAssemblyResult)res);

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
                    Logger.Log(LogLevel.Error, "Failed to load assembly: " + assemblyPath);

                    return (int)LoadAssemblyResult.NotFound;
                }

                // check if it has a dependency on the engine - if so, we need to verify the version is compatible
                AssemblyName? hyperionSharedDependency = RuntimeFeature.IsDynamicCodeSupported ? Array.Find(assembly.GetReferencedAssemblies(), (assemblyName) => assemblyName.Name == EngineAssemblies.Shared) : null;
                if (hyperionSharedDependency != null)
                {
                    // Verify the engine version (major, minor)
                    if (!VerifyEngineVersion(hyperionSharedDependency.Version?.ToString() ?? string.Empty, true, true, false))
                    {
                        Logger.Log(LogLevel.Error, "Assembly version does not match engine version");

                        return (int)LoadAssemblyResult.VersionMismatch;
                    }
                }

                if (IsHyperionAssembly(assembly))
                {
                    InitializeHyperionAssembly(assembly, isCoreAssembly != 0);
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Error loading assembly: {0}", ex);

                error = ex;

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
                    Logger.Log(LogLevel.Warning, "Failed to unload assembly: {0} not found", assemblyGuid);

                    foreach (var kv in AssemblyCache.Instance.GetAssemblies())
                    {
                        Logger.Log(LogLevel.Info, "Assembly: {0}", kv.Key);
                    }

                    result = false;
                }
                else
                {
                    Logger.Log(LogLevel.Info, "Unloading assembly: {0}...", assemblyGuid);

                    AssemblyCache.Instance.Remove(assemblyGuid);
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Error unloading assembly: {0}", ex);

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
                Debug.Assert(attribute != null);

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

        private static uint GetPrimitiveSize(Type type)
        {
            switch (Type.GetTypeCode(type))
            {
            case TypeCode.Boolean:
            case TypeCode.Byte:
            case TypeCode.SByte:
                return 1;
            case TypeCode.Char:
            case TypeCode.Int16:
            case TypeCode.UInt16:
                return 2;
            case TypeCode.Int32:
            case TypeCode.UInt32:
            case TypeCode.Single:
                return 4;
            case TypeCode.Int64:
            case TypeCode.UInt64:
            case TypeCode.Double:
                return 8;
            default:
                return (uint)HypMarshal.SizeOf(type);
            }
        }

        private static bool IsOperatingSystemConditionMet(string condition)
        {
            switch (condition)
            {
            case "IsWindows":
                return OperatingSystem.IsWindows();
            case "IsLinux":
                return OperatingSystem.IsLinux();
            case "IsMacOS":
                return OperatingSystem.IsMacOS();
            case "IsAndroid":
                return OperatingSystem.IsAndroid();
            case "IsIOS":
                return OperatingSystem.IsIOS();
            case "IsBrowser":
                return OperatingSystem.IsBrowser();
            default:
                Logger.Log(LogLevel.Error, "Unknown ClassBinding condition: {0}", condition);
                return false;
            }
        }

        private static unsafe IntPtr InitManagedClass(Type type, bool isCoreAssembly)
        {
            // Skip classes with the NoNativeClass attribute
            if (type.IsDefined(typeof(NoNativeClass), inherit: false))
            {
                Logger.Log(LogLevel.Verbose, "Skipping managed class for type: {0} due to NoNativeClass attribute", type.Name);

                return IntPtr.Zero;
            }

            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.GetByReference(type.Assembly)
                ?? AssemblyCache.Instance.Get(type.Assembly.GetName())
                ?? (type.Assembly.IsCollectible ? null : AssemblyCache.Instance.GetOrAdd(type.Assembly, isCoreAssembly: true));

            if (assemblyInstance == null)
            {
                throw new Exception("InitManagerClass: Failed to get assembly instance for type: " + type.Name + " from assembly: " + type.Assembly.FullName
                    + ", has the assembly been registered?");
            }

            Guid assemblyGuid = assemblyInstance.Guid;
            IntPtr assemblyPtr = assemblyInstance.AssemblyPtr;

            if (assemblyPtr == IntPtr.Zero)
            {
                throw new Exception("Assembly pointer is null for assembly: " + type.Assembly.FullName);
            }

            IntPtr foundClassObjectPtr = IntPtr.Zero;

            // Check if class has already been initialized
            if (ManagedClass_FindByTypeHash(assemblyPtr, type.TypeHandle.Value.ToInt64(), out foundClassObjectPtr))
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
            if (ManagedClass_FindByTypeHash(assemblyPtr, type.TypeHandle.Value.ToInt64(), out foundClassObjectPtr))
            {
                return foundClassObjectPtr;
            }

            ManagedClassDesc managedClassDesc = new ManagedClassDesc();

            string typeName = type.Name;
            IntPtr typeNamePtr = Marshal.StringToCoTaskMemUTF8(typeName);

            string? className = null;
            IntPtr classPtr = IntPtr.Zero;
            bool isOwnDynamicClass = false;

            TypeId typeId = TypeId.ForType(type);

            ClassBinding? classBindingAttribute = type.GetCustomAttribute<ClassBinding>(inherit: false);

            if (classBindingAttribute?.Condition is string condition && !IsOperatingSystemConditionMet(condition))
            {
                return IntPtr.Zero;
            }

            bool isScriptAssemblyType = !assemblyInstance.IsCoreAssembly;

            // a dynamic [ClassBinding] in a script assembly gets a reloadable class below, like any other script ObjectBase type
            if (classBindingAttribute != null && !(classBindingAttribute.IsDynamic && isScriptAssemblyType))
            {
                className = classBindingAttribute.Name;

                if (className == null || className.Length == 0)
                    className = typeName;

                if (classBindingAttribute.IsDynamic)
                {
                    IntPtr parentClassPtr = GetNativeParentClass(type);

                    if (parentClassPtr == IntPtr.Zero)
                        throw new Exception(string.Format("To create a dynamic Class for {0}, it must derive from ObjectBase, Script or a class with a valid ClassBinding attribute!", type.FullName));

                    classPtr = Class_CreateDynamicClass(ref typeId, className, parentClassPtr, type.IsAbstract, type.GetCustomAttribute<AssetBucketAttribute>(inherit: false)?.Name);

                    if (classPtr == IntPtr.Zero)
                        throw new Exception(string.Format("Failed to create a dynamic Class for type \"{0}\" (TypeId: {1})", type.Name, typeId));

                    isOwnDynamicClass = true;
                }
                else
                {
                    classPtr = Class_GetClassByName(className);

                    if (classPtr == IntPtr.Zero)
                        throw new Exception(string.Format("No Class found for \"{0}\"", className));
                }
            }
            else if (type.IsClass && (HasReflectedMembers(type) || (isScriptAssemblyType && typeof(ObjectBase).IsAssignableFrom(type))))
            {
                // Script subclasses with [HypField] / [HypProperty] members, and every ObjectBase subclass in a script assembly, get a native class of their own, keyed by type name.
                // A reloaded assembly creates a new one that supersedes the old, which lives on until its instances are gone.
                IntPtr parentClassPtr = GetNativeParentClass(type);

                if (parentClassPtr == IntPtr.Zero)
                {
                    Logger.Log(LogLevel.Warning, "{0} has reflected members but derives from neither Script nor a native class; they won't be saved or shown in the editor", type.FullName);
                }
                else
                {
                    classPtr = Class_CreateDynamicClass(ref typeId, typeName, parentClassPtr, type.IsAbstract, type.GetCustomAttribute<AssetBucketAttribute>(inherit: false)?.Name);

                    if (classPtr == IntPtr.Zero)
                        throw new Exception(string.Format("Failed to create a dynamic Class for type \"{0}\" (TypeId: {1})", type.Name, typeId));

                    isOwnDynamicClass = true;
                }
            }

            if (isOwnDynamicClass)
            {
                assemblyInstance.AddDynamicClass(classPtr);
                Class.RegisterDynamicClass(type, new Class(classPtr));
            }

            HypTypeEntry? typeEntry = HypTypeRegistry.Get(type);

            uint typeSize = 0;

            ManagedClassFlags managedClassFlags = ManagedClassFlags.None;

            if (type.IsClass)
            {
                managedClassFlags |= ManagedClassFlags.ClassType;
            }
            else if (type.IsValueType && !type.IsEnum)
            {
                managedClassFlags |= ManagedClassFlags.StructType;

                if (typeEntry != null && typeEntry.NativeSize != IntPtr.Zero)
                {
                    typeSize = (uint)((delegate*<int>)typeEntry.NativeSize)();
                }
            }
            else if (type.IsEnum)
            {
                managedClassFlags |= ManagedClassFlags.EnumType;
                typeSize = typeEntry != null && typeEntry.NativeSize != IntPtr.Zero
                    ? (uint)((delegate*<int>)typeEntry.NativeSize)()
                    : GetPrimitiveSize(Enum.GetUnderlyingType(type));
            }

            if (type.IsAbstract)
            {
                managedClassFlags |= ManagedClassFlags.Abstract;
            }

            ManagedClass_Create(ref assemblyGuid, assemblyPtr, classPtr, type.TypeHandle.Value.ToInt64(), typeNamePtr, typeSize, typeId, parentClassObjectPtr, (uint)managedClassFlags, out managedClassDesc);

            Marshal.FreeCoTaskMem(typeNamePtr);

            s_registrationStats.Classes++;

            ManagedAttributeHolder managedAttributeHolder = AllocAttributeHolder(assemblyGuid, assemblyPtr, type.GetCustomAttributes().ToArray());
            managedClassDesc.SetAttributes(ref managedAttributeHolder);
            managedAttributeHolder.Dispose();

            RegisterMethods(type, typeEntry, assemblyGuid, assemblyPtr, ref managedClassDesc);
            RegisterProperties(type, typeEntry, assemblyGuid, assemblyPtr, ref managedClassDesc);

            if (isOwnDynamicClass && typeEntry != null)
            {
                AddReflectedProperties(type, typeEntry, classPtr);
            }

            if (typeEntry != null && typeEntry.NewObject != IntPtr.Zero)
            {
                managedClassDesc.SetNewObjectFunction(ManagedInvokers.NewObjectGeneratedFptr, typeEntry.NewObject);
                s_registrationStats.Constructors++;
            }

            if (typeEntry != null && typeEntry.MarshalObject != IntPtr.Zero)
            {
                managedClassDesc.SetMarshalObjectFunction(ManagedInvokers.MarshalObjectGeneratedFptr, typeEntry.MarshalObject);
            }

            return managedClassDesc.ClassObjectPtr;
        }

        private static bool HasReflectedMembers(Type type)
        {
            for (Type? currentType = type; currentType != null; currentType = currentType.BaseType)
            {
                HypTypeEntry? entry = HypTypeRegistry.Get(currentType);

                if (entry != null && Array.Exists(entry.Properties, thunk => thunk.MemberType != null))
                {
                    return true;
                }
            }

            return false;
        }

        private static IntPtr GetNativeParentClass(Type type)
        {
            for (Type? baseType = type.BaseType; baseType != null; baseType = baseType.BaseType)
            {
                if (Class.TryGetDynamicClass(baseType, out Class dynamicClass))
                {
                    return dynamicClass.Address;
                }

                ClassBinding? classBindingAttribute = baseType.GetCustomAttribute<ClassBinding>(inherit: false);

                if (classBindingAttribute != null)
                {
                    return classBindingAttribute.GetClass(baseType).Address;
                }

                if (baseType.FullName == "Hyperion.Script")
                {
                    return Class_GetClassByName("ScriptInstance");
                }

                if (baseType == typeof(ObjectBase))
                {
                    return Class_GetClassByName("ObjectBase");
                }
            }

            return IntPtr.Zero;
        }

        // Inherited members are reflected on the base type's own class
        private static unsafe void AddReflectedProperties(Type type, HypTypeEntry typeEntry, IntPtr classPtr)
        {
            foreach (HypPropertyThunk thunk in typeEntry.Properties)
            {
                if (thunk.MemberType == null)
                {
                    continue;
                }

                TypeInfo typeInfo = GetNativeTypeInfo(thunk.MemberType, thunk.SampleValue);

                if (typeInfo.IsNull)
                {
                    Logger.Log(LogLevel.Warning, "Member {0}.{1} of type {2} has no native equivalent; it won't be saved or shown in the editor", type.Name, thunk.Name, thunk.MemberType.Name);

                    continue;
                }

                HypMemberAttribute? memberAttribute = thunk.MemberAttribute;
                IReadOnlyDictionary<string, object> nativeAttributes = memberAttribute != null ? memberAttribute.NativeAttributes : new Dictionary<string, object>();

                HypMemberAttributeDesc[] attributeDescs = new HypMemberAttributeDesc[nativeAttributes.Count];
                List<IntPtr> allocatedStrings = new List<IntPtr>();

                try
                {
                    int attributeIndex = 0;

                    foreach (KeyValuePair<string, object> kv in nativeAttributes)
                    {
                        IntPtr namePtr = Marshal.StringToHGlobalAnsi(kv.Key);
                        allocatedStrings.Add(namePtr);

                        HypMemberAttributeDesc desc = new HypMemberAttributeDesc { Name = namePtr };

                        switch (kv.Value)
                        {
                        case bool boolValue:
                            desc.Kind = 0;
                            desc.IntValue = boolValue ? 1 : 0;
                            break;
                        case int intValue:
                            desc.Kind = 1;
                            desc.IntValue = intValue;
                            break;
                        default:
                            desc.Kind = 2;
                            desc.StringValue = Marshal.StringToHGlobalAnsi(kv.Value.ToString());
                            allocatedStrings.Add(desc.StringValue);
                            break;
                        }

                        attributeDescs[attributeIndex++] = desc;
                    }

                    Class_AddManagedProperty(classPtr, thunk.Name, typeInfo.Address, attributeDescs, (uint)attributeDescs.Length, thunk.Setter != IntPtr.Zero);
                }
                finally
                {
                    foreach (IntPtr ptr in allocatedStrings)
                    {
                        Marshal.FreeHGlobal(ptr);
                    }
                }
            }
        }

        // The type of a member's boxed value, found by boxing its generated sample value the same way dynamic struct fields are
        private static TypeInfo GetNativeTypeInfo(Type memberType, object? sampleValue)
        {
            if (Nullable.GetUnderlyingType(memberType) != null)
            {
                return new TypeInfo();
            }

            // a null sample is only meaningful for ObjectBase types, which box as a null Handle<ObjectBase>
            if (sampleValue == null && !typeof(ObjectBase).IsAssignableFrom(memberType))
            {
                return new TypeInfo();
            }

            try
            {
                using BoxedValue boxed = new BoxedValue(sampleValue);

                return boxed.TypeInfo;
            }
            catch (Exception)
            {
                return new TypeInfo();
            }
        }

        // matches ManagedPropertyAttributeDesc in ClassBindings.cpp
        [StructLayout(LayoutKind.Sequential)]
        private struct HypMemberAttributeDesc
        {
            public IntPtr Name;
            public uint Kind;
            public int IntValue;
            public IntPtr StringValue;
        }

        private struct RegistrationStats
        {
            public int Classes;
            public int Methods;
            public int Properties;
            public int Constructors;
        }

        private static RegistrationStats s_registrationStats;

        private static unsafe void RegisterMethods(Type type, HypTypeEntry? typeEntry, Guid assemblyGuid, IntPtr assemblyPtr, ref ManagedClassDesc managedClassDesc)
        {
            HashSet<string> methodNames = new HashSet<string>();

            for (Type? currentType = type; currentType != null; currentType = currentType.BaseType)
            {
                HypTypeEntry? entry = currentType == type ? typeEntry : HypTypeRegistry.Get(currentType);

                if (entry == null)
                {
                    continue;
                }

                foreach (HypMethodThunk thunk in entry.Methods)
                {
                    if (!methodNames.Add(thunk.Name))
                    {
                        continue;
                    }

                    ManagedAttributeHolder managedAttributeHolder = AllocAttributeHolder(assemblyGuid, assemblyPtr, thunk.Attributes ?? Array.Empty<Attribute>());
                    managedClassDesc.AddMethod(thunk.Name, Guid.NewGuid(), ManagedInvokers.InvokeGeneratedFptr, thunk.Invoke, ref managedAttributeHolder);
                    managedAttributeHolder.Dispose();

                    s_registrationStats.Methods++;
                }
            }
        }

        private static unsafe void RegisterProperties(Type type, HypTypeEntry? typeEntry, Guid assemblyGuid, IntPtr assemblyPtr, ref ManagedClassDesc managedClassDesc)
        {
            HashSet<string> propertyNames = new HashSet<string>();

            for (Type? currentType = type; currentType != null; currentType = currentType.BaseType)
            {
                HypTypeEntry? entry = currentType == type ? typeEntry : HypTypeRegistry.Get(currentType);

                if (entry == null)
                {
                    continue;
                }

                foreach (HypPropertyThunk thunk in entry.Properties)
                {
                    if (!propertyNames.Add(thunk.Name))
                    {
                        continue;
                    }

                    ManagedAttributeHolder managedAttributeHolder = AllocAttributeHolder(assemblyGuid, assemblyPtr, thunk.Attributes ?? Array.Empty<Attribute>());
                    managedClassDesc.AddProperty(
                        thunk.Name,
                        Guid.NewGuid(),
                        thunk.Getter != IntPtr.Zero ? ManagedInvokers.InvokeGeneratedFptr : IntPtr.Zero,
                        thunk.Getter,
                        thunk.Setter != IntPtr.Zero ? ManagedInvokers.InvokeGeneratedFptr : IntPtr.Zero,
                        thunk.Setter,
                        ref managedAttributeHolder);
                    managedAttributeHolder.Dispose();

                    s_registrationStats.Properties++;
                }
            }
        }

        [UnmanagedCallersOnly]
        private static void AddObjectToCacheUnmanaged(IntPtr objectWrapperPtr, IntPtr outClassObjectPtr, IntPtr outObjectReferencePtr, byte weak)
        {
            AddObjectToCache(objectWrapperPtr, outClassObjectPtr, outObjectReferencePtr, weak != 0);
        }

        public static unsafe void AddObjectToCache(IntPtr objectWrapperPtr, IntPtr outClassObjectPtr, IntPtr outObjectReferencePtr, bool weak)
        {
            try
            {
                Marshal.WriteIntPtr(outClassObjectPtr, IntPtr.Zero);

                ref ObjectWrapper objectWrapperRef = ref Unsafe.AsRef<ObjectWrapper>((void*)objectWrapperPtr);
                ref ObjectReference objectReferenceRef = ref Unsafe.AsRef<ObjectReference>((void*)outObjectReferencePtr);

                object obj = objectWrapperRef.obj;

                if (obj == null)
                    throw new ArgumentNullException(nameof(obj));

                if (objectReferenceRef.WeakHandle != IntPtr.Zero || objectReferenceRef.StrongHandle != IntPtr.Zero)
                    throw new InvalidOperationException("ObjectReference already has handles assigned");

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
                if (!ManagedClass_FindByTypeHash(assemblyPtr, type.TypeHandle.Value.ToInt64(), out pClass))
                {
                    throw new Exception("ManagedClass not found for Type " + type.Name + " from assembly: " + type.Assembly.FullName + ", has the assembly been registered? Ensure the class or struct is public.");
                }

                GCHandle gcHandleWeak = GCHandle.Alloc(obj, GCHandleType.Weak);
                GCHandle? gcHandleStrong = null;

                if (!weak)
                    gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);

                s_liveObjectCounts.AddOrUpdate(pClass, 1, (_, count) => count + 1);

                Marshal.WriteIntPtr(outClassObjectPtr, pClass);

                // @NOTE: reassign ref
                objectReferenceRef = new ObjectReference
                {
                    WeakHandle = GCHandle.ToIntPtr(gcHandleWeak),
                    StrongHandle = gcHandleStrong.HasValue ? GCHandle.ToIntPtr(gcHandleStrong.Value) : IntPtr.Zero
                };
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown adding object to cache: {0}", ex);
            }
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

        // Finalizers release native objects, so a second collection picks up what only they kept alive
        [UnmanagedCallersOnly]
        public static unsafe void CollectGarbageAndWait()
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }

        [UnmanagedCallersOnly]
        public static unsafe void GetAssemblyPointer(IntPtr assemblyObjectReferencePtr, IntPtr outAssemblyPtr)
        {
            Marshal.WriteIntPtr(outAssemblyPtr, IntPtr.Zero);

            ref ObjectReference objectReference = ref Unsafe.AsRef<ObjectReference>((void*)assemblyObjectReferencePtr);

            Assembly? assembly = (Assembly?)objectReference.LoadObject();
            Assert.Throw(assembly != null);

            if (assembly == null)
                return;

            AssemblyInstance? assemblyInstance = AssemblyCache.Instance.Get(assembly);
            Assert.Throw(assemblyInstance != null);

            if (assemblyInstance == null)
                return;

            Marshal.WriteIntPtr(outAssemblyPtr, assemblyInstance.AssemblyPtr);
        }

        [UnmanagedCallersOnly]
        public static unsafe void CleanupOnShutdown()
        {
            // NativeAOT can't create the load context, and never loaded anything into it
            if (RuntimeFeature.IsDynamicCodeSupported)
            {
                AssemblyInstance.CoreAssemblyLoadContext.Unload();
            }

            AssemblyCache.Instance.Clear();

            GC.Collect(0, GCCollectionMode.Forced, blocking: true, compacting: true);
        }

        [UnmanagedCallersOnly]
        public static void RemoveObjectFromCache(IntPtr classPtr)
        {
            if (classPtr == IntPtr.Zero)
                return;

            s_liveObjectCounts.AddOrUpdate(classPtr, 0, (_, count) => Math.Max(0, count - 1));
        }

        [UnmanagedCallersOnly]
        public static unsafe void FreeObjectReference(IntPtr objectReferencePtr)
        {
            if (objectReferencePtr == IntPtr.Zero)
                return;

            try
            {
                ref ObjectReference objectReference = ref Unsafe.AsRef<ObjectReference>((void*)objectReferencePtr);
                objectReference.Dispose();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown freeing object reference: {0}", ex);
            }
        }

        [UnmanagedCallersOnly]
        public static long GetTotalMemory()
        {
            return GC.GetTotalMemory(false);
        }

        [UnmanagedCallersOnly]
        public static void SimTick(float deltaTime)
        {
            try
            {
                SimThreadSynchronizationContext.Instance.Tick(deltaTime);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown on sim thread tick! {0}", ex);
            }
        }

        [UnmanagedCallersOnly]
        public static unsafe int QueryManagedObjectCounts(IntPtr* outPairs, int maxPairs)
        {
            int written = 0;
            foreach (var kvp in s_liveObjectCounts)
            {
                if (written >= maxPairs)
                    break;
                if (kvp.Value <= 0)
                    continue;

                outPairs[written * 2 + 0] = kvp.Key;
                outPairs[written * 2 + 1] = (IntPtr)kvp.Value;
                written++;
            }
            return written;
        }

        public static void HandleUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            Logger.Log(LogLevel.Error, "Unhandled managed exception: {0}\n\n{1}", ((Exception)e.ExceptionObject).Message, ((Exception)e.ExceptionObject).StackTrace);

            // rethrow in debug mode
#if DEBUG
            throw (Exception)e.ExceptionObject;
#else

            MessageBox.Critical()
                .Title("Uncaught Exception")
                .Text("An unhandled exception occurred in managed code!\n"
                    + ((Exception)e.ExceptionObject).Message + "\n\n"
                    + "Check the logs for more details.")
                .Button("OK", () => { })
                .Show();
#endif
        }

        [DllImport("hyperion", EntryPoint = "ManagedClass_Create")]
        private static extern void ManagedClass_Create(ref Guid assemblyGuid, IntPtr assemblyPtr, IntPtr classPtr, long typeHash, IntPtr typeNamePtr, uint typeSize, TypeId typeId, IntPtr parentClassPtr, uint managedClassFlags, [Out] out ManagedClassDesc outDesc);

        [DllImport("hyperion", EntryPoint = "ManagedClass_FindByTypeHash")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ManagedClass_FindByTypeHash([In] IntPtr assemblyPtr, long typeHash, [Out] out IntPtr outManagedClassObjectPtr);

        [DllImport("hyperion", EntryPoint = "Class_GetClassByName")]
        private static extern IntPtr Class_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "Class_GetClassByTypeId")]
        private static extern IntPtr Class_GetClassByTypeId([In] ref TypeId typeId);

        [DllImport("hyperion", EntryPoint = "Class_GetClassForManagedClass")]
        private static extern IntPtr Class_GetClassForManagedClass([In] IntPtr pClass);

        [DllImport("hyperion", EntryPoint = "Class_CreateDynamicClass")]
        private static extern IntPtr Class_CreateDynamicClass([In] ref TypeId typeId, [MarshalAs(UnmanagedType.LPStr)] string name, [In] IntPtr parentClassClassPtr, [MarshalAs(UnmanagedType.I1)] bool isAbstract, [MarshalAs(UnmanagedType.LPStr)] string? assetBucket);

        [DllImport("hyperion", EntryPoint = "Class_AddManagedProperty")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool Class_AddManagedProperty([In] IntPtr classPtr, [MarshalAs(UnmanagedType.LPStr)] string name, [In] IntPtr valueTypeInfo, [In] HypMemberAttributeDesc[] attributes, uint numAttributes, [MarshalAs(UnmanagedType.I1)] bool canSet);

        [DllImport("hyperion", EntryPoint = "NativeInterop_VerifyEngineVersion")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool NativeInterop_VerifyEngineVersion(uint assemblyEngineVersion, bool major, bool minor, bool patch);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetHostedScriptAssembly")]
        private static extern void NativeInterop_SetHostedScriptAssembly(IntPtr assemblyPtr);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetAddObjectToCacheFunction")]
        private static extern void NativeInterop_SetAddObjectToCacheFunction(IntPtr addObjectToCacheFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetSetKeepAliveFunction")]
        private static extern unsafe void NativeInterop_SetSetKeepAliveFunction(void* setKeepAliveFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetTriggerGCFunction")]
        private static extern unsafe void NativeInterop_SetTriggerGCFunction(void* setTriggerGcFunctionFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetCollectGarbageAndWaitFunction")]
        private static extern unsafe void NativeInterop_SetCollectGarbageAndWaitFunction(void* collectGarbageAndWaitFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetGetAssemblyPointerFunction")]
        private static extern unsafe void NativeInterop_SetGetAssemblyPointerFunction(void* getAssemblyPointerFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetCleanupOnShutdownFunction")]
        private static extern unsafe void NativeInterop_SetCleanupOnShutdownFunction(void* cleanupOnShutdownFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetRemoveObjectFromCacheFunction")]
        private static extern unsafe void NativeInterop_SetRemoveObjectFromCacheFunction(void* removeObjectFromCacheFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetFreeObjectReferenceFunction")]
        private static extern unsafe void NativeInterop_SetFreeObjectReferenceFunction(void* freeObjectReferenceFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetQueryManagedObjectCountsFunction")]
        private static extern unsafe void NativeInterop_SetQueryManagedObjectCountsFunction(void* queryManagedObjectCountsFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetGetTotalMemoryFunction")]
        private static extern unsafe void NativeInterop_SetGetTotalMemoryFunction(void* getTotalMemoryFunction);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetSimTickFunction")]
        private static extern unsafe void NativeInterop_SetSimTickFunction(void* simTickFunction);

        [DllImport("hyperion")]
        private static extern int NativeInterop_NewAssembly(Guid guid, out IntPtr outAssemblyPtr);

        [DllImport("hyperion")]
        private static extern void NativeInterop_FreeAssembly(IntPtr assemblyPtr);
    }
}
