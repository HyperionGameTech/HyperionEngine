using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public delegate void InvokeMethodDelegate(Guid managedMethodGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr);

    public class NativeInterop
    {
        public static InvokeMethodDelegate InvokeMethodDelegate = InvokeMethod;

        [UnmanagedCallersOnly]
        public static void InitializeAssembly(IntPtr outAssemblyGuid, IntPtr classHolderPtr, IntPtr assemblyPathStringPtr)
        {
            // Create a managed string from the pointer
            string assemblyPath = Marshal.PtrToStringAnsi(assemblyPathStringPtr);

            if (AssemblyCache.Instance.Get(assemblyPath) != null)
            {
                // throw new Exception("Assembly already loaded: " + assemblyPath + " (" + AssemblyCache.Instance.Get(assemblyPath).Guid + ")");

                Logger.Log(LogType.Info, "Assembly already loaded: {0}", assemblyPath);

                return;
            }

            Guid assemblyGuid = Guid.NewGuid();
            Marshal.StructureToPtr(assemblyGuid, outAssemblyGuid, false);

            Logger.Log(LogType.Info, "Loading assembly: {0}...", assemblyPath);

            AssemblyInstance assemblyInstance = AssemblyCache.Instance.Add(assemblyGuid, assemblyPath);
            Assembly? assembly = assemblyInstance.Assembly;

            if (assembly == null)
            {
                throw new Exception("Failed to load assembly: " + assemblyPath);
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
                    throw new Exception("Assembly version does not match engine version");
                }
            }

            Type[] types = assembly.GetTypes();

            foreach (Type type in types)
            {
                if (type.IsClass)
                {
                    InitManagedClass(assemblyGuid, classHolderPtr, type);
                }
            }

            NativeInterop_SetInvokeMethodFunction(ref assemblyGuid, classHolderPtr, Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeMethod));
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
        public static unsafe void AddObjectToCache(IntPtr assemblyGuidPtr, IntPtr objectGuidPtr, IntPtr objectPtr, IntPtr outManagedObjectPtr)
        {
            Guid assemblyGuid = Marshal.PtrToStructure<Guid>(assemblyGuidPtr);
            Guid objectGuid = Marshal.PtrToStructure<Guid>(objectGuidPtr);

            // read object as reference
            ref object obj = ref System.Runtime.CompilerServices.Unsafe.AsRef<object>(objectPtr.ToPointer());

            ManagedObject managedObject = ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj);

            // write managedObject to outManagedObjectPtr
            Marshal.StructureToPtr(managedObject, outManagedObjectPtr, false);
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

            Type baseType = type.BaseType;

            if (baseType != null)
            {
                parentClass = InitManagedClass(assemblyGuid, classHolderPtr, baseType);
            }

            string typeName = type.Name;
            IntPtr typeNamePtr = Marshal.StringToHGlobalAnsi(typeName);

            IntPtr hypClassPtr = IntPtr.Zero;

            var classAttributes = type.GetCustomAttributes();//typeof(HypClassBinding), false);

            foreach (var attr in classAttributes)
            {
                if (attr.GetType().Name == "HypClassBinding")
                {
                    hypClassPtr = HypClass_GetClassByName(typeName);

                    if (hypClassPtr == IntPtr.Zero)
                    {
                        throw new Exception(string.Format("No HypClass found for \"{0}\"", typeName));
                    }

                    break;
                }
            }

            // foreach (HypClassBinding attribute in classAttributes)
            // {
            //     hypClassPtr = HypClass_GetClassByName(attribute.Name);

            //     if (hypClassPtr == IntPtr.Zero)
            //     {
            //         throw new Exception(string.Format("No HypClass found for {0}!", attribute.Name));
            //     }

            //     break;
            // }

            ; // @TODO Use attribute on class
            Console.WriteLine("HypClass for {0} = {1}", typeName, hypClassPtr);

            ManagedClass_Create(ref assemblyGuid, classHolderPtr, hypClassPtr, type.GetHashCode(), typeNamePtr, parentClass?.classObjectPtr ?? IntPtr.Zero, out managedClass);

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
            managedClass.NewObjectFunction = new NewObjectDelegate(() =>
            {
                // Allocate the object
                object obj = RuntimeHelpers.GetUninitializedObject(type);
                // GCHandle objHandle = GCHandle.Alloc(obj);

                // Call the constructor
                ConstructorInfo constructorInfo = type.GetConstructor(BindingFlags.Public | BindingFlags.Instance, null, Type.EmptyTypes, null);

                if (constructorInfo == null)
                {
                    throw new Exception("Failed to find empty constructor for type: " + type.Name);
                }

                constructorInfo.Invoke(obj, null);

                Guid objectGuid = Guid.NewGuid();

                // @TODO: Reduce complexity - this is a bit of a mess between C# adn C++ interop
                // ManagedObject managedObject = new ManagedObject();

                // NativeInterop_AddObjectToCache(ref assemblyGuid, ref objectGuid, GCHandle.ToIntPtr(objHandle), out managedObject);

                // objHandle.Free();

                // return managedObject;
                
                return ManagedObjectCache.Instance.AddObject(assemblyGuid, objectGuid, obj);
            });

            managedClass.FreeObjectFunction = new FreeObjectDelegate(FreeObject);

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

                // Helper for strings
                if (paramType == typeof(string))
                {
                    parameters[i] = Marshal.PtrToStringAnsi(Marshal.ReadIntPtr(paramAddress));

                    continue;
                }

                if (paramType == typeof(IntPtr))
                {
                    parameters[i] = Marshal.ReadIntPtr(paramAddress);

                    continue;
                }

                if (paramType.IsValueType)
                {
                    parameters[i] = Marshal.PtrToStructure(paramAddress, paramType);

                    continue;
                }

                if (paramType.IsPointer)
                {
                    throw new NotImplementedException("Pointer parameter type not implemented");
                }

                if (paramType.IsByRef)
                {
                    throw new NotImplementedException("ByRef parameter type not implemented");
                }

                throw new NotImplementedException("Parameter type not implemented");
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

                thisObject = storedObject.obj;
            }

            object? returnValue = methodInfo.Invoke(thisObject, parameters);

            // If returnType is an enum we need to get the underlying type and cast the value to it
            if (returnType.IsEnum)
            {
                returnType = Enum.GetUnderlyingType(returnType);
                returnValue = Convert.ChangeType(returnValue, returnType);
            }

            if (returnType == typeof(void))
            {
                // No need to fill out the outPtr
                return;
            }

            if (returnType == typeof(string))
            {
                throw new NotImplementedException("String return type not implemented");
            }

            if (returnType == typeof(bool))
            {
                Marshal.WriteByte(outPtr, (byte)((bool)returnValue ? 1 : 0));

                return;
            }

            // write the return value to the outPtr
            // there MUST be enough space allocated at the outPtr,
            // or else this will cause memory corruption
            if (returnType.IsValueType)
            {
                Marshal.StructureToPtr(returnValue, outPtr, false);

                return;
            }

            if (returnType == typeof(IntPtr))
            {
                Marshal.WriteIntPtr(outPtr, (IntPtr)returnValue);

                return;
            }

            throw new NotImplementedException("Return type not implemented");
        }

        public static void FreeObject(ManagedObject obj)
        {
            if (!ManagedObjectCache.Instance.RemoveObject(obj.guid))
            {
                throw new Exception("Failed to remove object from cache: " + obj.guid);
            }
        }

        [DllImport("hyperion", EntryPoint = "ManagedClass_Create")]
        private static extern void ManagedClass_Create(ref Guid assemblyGuid, IntPtr classHolderPtr, IntPtr hypClassPtr, int typeHash, IntPtr typeNamePtr, IntPtr parentClassPtr, [Out] out ManagedClass result);

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
