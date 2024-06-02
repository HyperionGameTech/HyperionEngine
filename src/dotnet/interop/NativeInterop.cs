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
        public static void InitializeAssembly(IntPtr classHolderPtr, IntPtr assemblyPathStringPtr)
        {
            // Create a managed string from the pointer
            string assemblyPath = Marshal.PtrToStringAnsi(assemblyPathStringPtr);

            Logger.Log(LogType.Info, "Loading assembly: {0}...", assemblyPath);

            Assembly assembly = Assembly.LoadFrom(assemblyPath);

            if (assembly == null)
            {
                throw new Exception("Failed to load assembly: " + assemblyPath);
            }

#if DEBUG // Load all referenced assemblies to ensure nothing will crash later
            Logger.Log(LogType.Info, "Loaded assembly: {0}, with {1} referenced assemblies.", assembly.FullName, assembly.GetReferencedAssemblies().Length);

            for (int i = 0; i < assembly.GetReferencedAssemblies().Length; i++)
            {
                Logger.Log(LogType.Info, "Found referenced assembly: {0}", assembly.GetReferencedAssemblies()[i].Name);

                // Ensure the referenced assembly is found
                Assembly.Load(assembly.GetReferencedAssemblies()[i]);
            }
#endif

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
                    InitManagedClass(classHolderPtr, type);
                }
            }

            NativeInterop_SetInvokeMethodFunction(classHolderPtr, Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeMethod));
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

        private static ManagedClass InitManagedClass(IntPtr classHolderPtr, Type type)
        {
            string typeName = type.Name;

            IntPtr typeNamePtr = Marshal.StringToHGlobalAnsi(typeName);

            ManagedClass managedClass = new ManagedClass();
            ManagedClass_Create(classHolderPtr, type.GetHashCode(), typeNamePtr, out managedClass);

            Marshal.FreeHGlobal(typeNamePtr);

            Dictionary<string, MethodInfo> methods = new Dictionary<string, MethodInfo>();
            CollectMethods(type, methods);

            foreach (var item in methods)
            {
                // Get all custom attributes for the method
                object[] attributes = item.Value.GetCustomAttributes(false /* inherit */);

                List<string> attributeNames = new List<string>();

                foreach (object attribute in attributes)
                {
                    // Add full qualified name of the attribute
                    attributeNames.Add(attribute.GetType().FullName);
                }

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                Guid guid = Guid.NewGuid();
                managedClass.AddMethod(item.Key, guid, attributeNames.ToArray());

                ManagedMethodCache.Instance.AddMethod(guid, item.Value);
            }

            // Add new object, free object delegates
            managedClass.NewObjectFunction = new NewObjectDelegate(() =>
            {
                // Allocate the object
                object obj = RuntimeHelpers.GetUninitializedObject(type);

                // Call the constructor
                ConstructorInfo constructorInfo = type.GetConstructor(BindingFlags.Public | BindingFlags.Instance, null, Type.EmptyTypes, null);

                if (constructorInfo == null)
                {
                    throw new Exception("Failed to find empty constructor for type: " + type.Name);
                }

                constructorInfo.Invoke(obj, null);

                return ManagedObjectCache.Instance.AddObject(obj);
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
            ManagedObjectCache.Instance.RemoveObject(obj.guid);
        }

        [DllImport("hyperion", EntryPoint = "ManagedClass_Create")]
        private static extern void ManagedClass_Create(IntPtr classHolderPtr, int typeHash, IntPtr typeNamePtr, [Out] out ManagedClass result);

        [DllImport("hyperion", EntryPoint = "NativeInterop_VerifyEngineVersion")]
        private static extern bool NativeInterop_VerifyEngineVersion(uint assemblyEngineVersion, bool major, bool minor, bool patch);

        [DllImport("hyperion", EntryPoint = "NativeInterop_SetInvokeMethodFunction")]
        private static extern void NativeInterop_SetInvokeMethodFunction(IntPtr classHolderPtr, IntPtr invokeMethodPtr);
    }
}
