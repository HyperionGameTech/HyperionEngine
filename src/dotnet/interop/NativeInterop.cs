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
        public static void TestFunction()
        {
            Console.WriteLine("Hello from C#!");

            Logger.Log(LogType.Info, "Hello from C#!");
        }

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

            Logger.Log(LogType.Info, "Loaded assembly: {0}, with {1} references assemblies.", assembly.FullName, assembly.GetReferencedAssemblies().Length);

            for (int i = 0; i < assembly.GetReferencedAssemblies().Length; i++)
            {
                Logger.Log(LogType.Info, "Found assembly: {0}", assembly.GetReferencedAssemblies()[i].Name);
            }

            AssemblyName hyperionCoreDependency = Array.Find(assembly.GetReferencedAssemblies(), (assemblyName) => assemblyName.Name == "HyperionCore");

            if (hyperionCoreDependency == null)
            {
                throw new Exception("Failed to find HyperionCore dependency");
            }

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

        private static ManagedClass InitManagedClass(IntPtr classHolderPtr, Type type)
        {
            string typeName = type.Name;

            Console.WriteLine("Initializing managed class: " + typeName);

            IntPtr typeNamePtr = Marshal.StringToHGlobalAnsi(typeName);

            ManagedClass managedClass = new ManagedClass();
            ManagedClass_Create(classHolderPtr, type.GetHashCode(), typeNamePtr, out managedClass);

            Marshal.FreeHGlobal(typeNamePtr);

            MethodInfo[] methodInfos = type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.FlattenHierarchy);

            foreach (MethodInfo methodInfo in methodInfos)
            {
                // Skip constructors
                if (methodInfo.IsConstructor)
                {
                    continue;
                }

                // Get all custom attributes for the method
                object[] attributes = methodInfo.GetCustomAttributes(false /* inherit */);

                List<string> attributeNames = new List<string>();

                foreach (object attribute in attributes)
                {
                    // Add full qualified name of the attribute
                    attributeNames.Add(attribute.GetType().FullName);
                }

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                Guid guid = Guid.NewGuid();
                managedClass.AddMethod(methodInfo.Name, guid, attributeNames.ToArray());

                ManagedMethodCache.Instance.AddMethod(guid, methodInfo);
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

        private static void HandleParameters(IntPtr paramsPtr, MethodInfo methodInfo, out object?[] parameters)
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

                parameters[i] = Marshal.PtrToStructure(paramAddress, paramType);
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

            object returnValue = methodInfo.Invoke(thisObject, parameters);

            if (returnType == typeof(void))
            {
                // No need to fill out the outPtr
                return;
            }

            if (returnType == typeof(string))
            {
                throw new NotImplementedException("String return type not implemented");
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
