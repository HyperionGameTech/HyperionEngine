using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

public delegate IntPtr InitializeAssemblyDelegate(IntPtr classHolderPtr, string assemblyPath);

namespace Hyperion
{
    public delegate IntPtr InvokeMethodDelegate(IntPtr managedMethodPtr, IntPtr paramsPtr);

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedMethod
    {
        public IntPtr methodInfoPtr;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedClass
    {
        private int typeHash;
        private IntPtr classObjectPtr;

        public ManagedMethod AddMethod(string methodName, IntPtr methodInfoPtr)
        {
            return ManagedClass_AddMethod(this, methodName, methodInfoPtr);
        }

        // Add a function pointer to the managed class
        [DllImport("libhyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern ManagedMethod ManagedClass_AddMethod(ManagedClass managedClass, string methodName, IntPtr methodInfoPtr);

        [DllImport("libhyperion", EntryPoint = "ManagedClass_GetName")]
        private static extern string ManagedClass_GetName(ManagedClass managedClass);
    }

    internal struct StoredManagedMethod
    {
        public MethodInfo methodInfo;
    }

    public class NativeInterop
    {
        private static Dictionary<Type, List<StoredManagedMethod>> delegateCache = new Dictionary<Type, List<StoredManagedMethod>>();

        public static InitializeAssemblyDelegate InitializeAssemblyDelegate = InitializeAssembly;
        public static InvokeMethodDelegate InvokeMethodDelegate = InvokeMethod;

        public static IntPtr InitializeAssembly(IntPtr classHolderPtr, string assemblyPath)
        {
            Logger.Log(LogType.Debug, "Initializing assembly: " + assemblyPath + " ptr: " + classHolderPtr);

            Assembly assembly = Assembly.LoadFrom(assemblyPath);

            if (assembly == null)
            {
                throw new Exception("Failed to load assembly: " + assemblyPath);
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

            return IntPtr.Zero;

            // So C++ can use InvokeMethod to call C# methods
            // NativeInterop_Initialize(Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeMethod));
        }

        private static ManagedClass InitManagedClass(IntPtr classHolderPtr, Type type)
        {
            Logger.Log(LogType.Debug, "Initializing managed class: " + type.Name + " ptr: " + classHolderPtr);

            ManagedClass managedClass = ManagedClass_Create(classHolderPtr, type.GetHashCode(), type.Name);

            MethodInfo[] methodInfos = type.GetMethods(BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static);

            foreach (MethodInfo methodInfo in methodInfos)
            {
                Logger.Log(LogType.Debug, "Adding method: " + methodInfo.Name);

                // get intptr to the MethodInfo object
                IntPtr methodInfoPtr = Marshal.UnsafeAddrOfPinnedArrayElement(Unsafe.As<MethodInfo[]>(new MethodInfo[] { methodInfo }), 0);

                ManagedMethod managedMethod = managedClass.AddMethod(methodInfo.Name, methodInfoPtr);

                if (!delegateCache.ContainsKey(type))
                {
                    delegateCache.Add(type, new List<StoredManagedMethod>());
                }

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                delegateCache[type].Add(new StoredManagedMethod
                {
                    methodInfo = methodInfo
                });
            }

            return managedClass;
        }

        public static unsafe IntPtr InvokeMethod(IntPtr managedMethodPtr, IntPtr paramsPtr)
        {
            ManagedMethod managedMethod = Unsafe.Read<ManagedMethod>((void*)managedMethodPtr);
            MethodInfo* methodInfo = (MethodInfo*)managedMethod.methodInfoPtr;

            // @TODO: params, 'this', return value
            object[]? parameters = null;

            int numParams = methodInfo->GetParameters().Length;

            if (numParams > 0) {
                parameters = new object[numParams];

                int paramsOffset = 0;

                for (int i = 0; i < numParams; i++)
                {
                    Type paramType = methodInfo->GetParameters()[i].ParameterType;

                    // params is stored as void**
                    IntPtr paramAddress = Marshal.ReadIntPtr(paramsPtr, paramsOffset);
                    paramsOffset += IntPtr.Size;

                    try
                    {
                        // if (paramType.IsValueType)
                        // {
                        //     parameters[i] = Marshal.PtrToStructure(paramAddress, paramType);

                        //     continue;
                        // }

                        if (paramType == typeof(string))
                        {
                            // paramAddress is a void* pointing to const char* (another layer of indirection)
                            parameters[i] = Marshal.PtrToStringAnsi(Marshal.ReadIntPtr(paramAddress));

                            // parameters[i] = Marshal.PtrToStringAnsi(paramAddress);

                            continue;
                        }

                        // if (paramType == typeof(IntPtr))
                        // {
                        //     parameters[i] = paramAddress;

                        //     continue;
                        // }

                        parameters[i] = Marshal.PtrToStructure(paramAddress, paramType);
                    }
                    catch (Exception e)
                    {
                        throw new Exception("Failed to marshal parameter: " + e.Message);
                    }

                }
            }

            methodInfo->Invoke(null, parameters);

            return IntPtr.Zero;
        }

        [DllImport("libhyperion", EntryPoint = "ManagedClass_Create")]
        private static extern ManagedClass ManagedClass_Create(IntPtr classHolderPtr, int typeHash, string typeName);

        [DllImport("libhyperion", EntryPoint = "NativeInterop_Initialize")]
        private static extern void NativeInterop_Initialize(IntPtr invokeMethodPtr);

        [DllImport("libhyperion", EntryPoint = "NativeInterop_SetInvokeMethodFunction")]
        private static extern void NativeInterop_SetInvokeMethodFunction(IntPtr classHolderPtr, IntPtr invokeMethodPtr);
    }
}