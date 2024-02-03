using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

public delegate void InitializeAssemblyDelegate(IntPtr classHolderPtr, string assemblyPath);

namespace Hyperion
{
    public delegate void InvokeMethodDelegate(IntPtr managedMethodPtr, IntPtr thisPtr, IntPtr paramsPtr, IntPtr outPtr);

    internal class StoredManagedObject : IDisposable
    {
        public Guid guid;
        public object obj;
        public GCHandle gcHandle;

        public StoredManagedObject(Guid guid, object obj)
        {
            this.guid = guid;
            this.obj = obj;
            this.gcHandle = GCHandle.Alloc(this.obj);
        }

        public void Dispose()
        {
            gcHandle.Free();
        }

        public ManagedObject ToManagedObject()
        {
            return new ManagedObject
            {
                guid = guid,
                ptr = GCHandle.ToIntPtr(gcHandle)
            };
        }
    }

    internal struct StoredManagedMethod
    {
        public MethodInfo methodInfo;
    }

    public class NativeInterop
    {
        public static InitializeAssemblyDelegate InitializeAssemblyDelegate = InitializeAssembly;
        public static InvokeMethodDelegate InvokeMethodDelegate = InvokeMethod;

        public static void InitializeAssembly(IntPtr classHolderPtr, string assemblyPath)
        {
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
        }

        private static ManagedClass InitManagedClass(IntPtr classHolderPtr, Type type)
        {
            ManagedClass managedClass = ManagedClass_Create(classHolderPtr, type.GetHashCode(), type.Name);

            MethodInfo[] methodInfos = type.GetMethods(BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static);

            foreach (MethodInfo methodInfo in methodInfos)
            {
                // Skip constructors
                if (methodInfo.IsConstructor)
                {
                    continue;
                }

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                Guid guid = Guid.NewGuid();
                ManagedMethod managedMethod = managedClass.AddMethod(methodInfo.Name, guid);

                ManagedMethodCache.Instance.AddMethod(guid, methodInfo);
            }

            // Add new object, free object delegates
            NewObjectDelegate newObjectDelegate = new NewObjectDelegate(() =>
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

                ManagedObject managedObject = ManagedObjectCache.Instance.AddObject(obj);

                return managedObject;
            });

            FreeObjectDelegate freeObjectDelegate = new FreeObjectDelegate((ManagedObject obj) =>
            {
                StoredManagedObject? storedObject = ManagedObjectCache.Instance.GetObject(obj.guid);

                if (storedObject == null)
                {
                    throw new Exception("Failed to find object with guid: " + obj.guid);
                }

                ManagedObjectCache.Instance.RemoveObject(obj.guid);
            });

            managedClass.NewObjectFunction = newObjectDelegate;
            managedClass.FreeObjectFunction = freeObjectDelegate;

            return managedClass;
        }

        private static void HandleParameters(IntPtr paramsPtr, MethodInfo methodInfo, out object?[] parameters)
        {
            int numParams = methodInfo.GetParameters().Length;

            if (numParams == 0)
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

        public static unsafe void InvokeMethod(IntPtr managedMethodPtr, IntPtr thisPtr, IntPtr paramsPtr, IntPtr outPtr)
        {
            ManagedMethod managedMethod = Unsafe.Read<ManagedMethod>((void*)managedMethodPtr);
            MethodInfo methodInfo = ManagedMethodCache.Instance.GetMethod(managedMethod.guid);

            Type returnType = methodInfo.ReturnType;
            Type thisType = methodInfo.DeclaringType;

            object[]? parameters = null;
            HandleParameters(paramsPtr, methodInfo, out parameters);

            object? thisObject = null;
            GCHandle? thisGCHandle = null;

            if (!methodInfo.IsStatic)
            {
                thisGCHandle = GCHandle.FromIntPtr(thisPtr);
                thisObject = thisGCHandle.Value.Target;
            }

            object returnValue = methodInfo.Invoke(thisObject, parameters);

            if (thisGCHandle != null)
            {
                thisGCHandle.Value.Free();
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

            // write the return value to the outPtr
            // there MUST be enough space allocated at the outPtr
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

        [DllImport("libhyperion", EntryPoint = "ManagedClass_Create")]
        private static extern ManagedClass ManagedClass_Create(IntPtr classHolderPtr, int typeHash, string typeName);

        [DllImport("libhyperion", EntryPoint = "NativeInterop_Initialize")]
        private static extern void NativeInterop_Initialize(IntPtr invokeMethodPtr);

        [DllImport("libhyperion", EntryPoint = "NativeInterop_SetInvokeMethodFunction")]
        private static extern void NativeInterop_SetInvokeMethodFunction(IntPtr classHolderPtr, IntPtr invokeMethodPtr);
    }
}