using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Collections.Concurrent;

namespace Hyperion
{
    public class HypObject : IDisposable
    {
        public IntPtr _classPtr;
        public IntPtr _nativeAddress;

        protected HypObject()
        {
            bool initiatedFromManagedSide = _nativeAddress == IntPtr.Zero;

            if (initiatedFromManagedSide)
            {
                Type type = this.GetType();

                Class cls = Class.GetClass(type);

                if (cls == Class.Invalid)
                {
                    throw new Exception("Invalid Class returned from ClassBinding attribute");
                }

                if (!cls.IsReferenceCounted)
                {
                    throw new Exception("Can only create instances of reference counted Class objects (using Handle<T>) from managed code");
                }

                // Need to add this to managed object cache,
                // pass to CreateInstance() so the HypObject in C++ knows not to create another of this..
                GCHandle gcHandle = GCHandle.Alloc(this, GCHandleType.Normal);

                ObjectWrapper objectWrapper = new ObjectWrapper { obj = this };
                ObjectReference objectReference = new ObjectReference();

                unsafe
                {
                    IntPtr objectWrapperPtr = (IntPtr)Unsafe.AsPointer(ref objectWrapper);
                    IntPtr objectReferencePtr = (IntPtr)Unsafe.AsPointer(ref objectReference);

                    IntPtr pClass = IntPtr.Zero;

                    NativeInterop_AddObjectToCache(objectWrapperPtr, out pClass, objectReferencePtr, isWeak: true);

#if DEBUG
                    if (pClass == IntPtr.Zero)
                    {
                        gcHandle.Free();
                        throw new Exception("Failed to add object to cache -- pClass is null");
                    }

                    if (!objectReference.IsValid)
                    {
                        gcHandle.Free();
                        throw new Exception("Failed to add object to cache -- objectReference is invalid");
                    }
#endif

                    _classPtr = cls.Address;
                    
                    HypObject_Initialize(_classPtr, pClass, ref objectReference, out _nativeAddress);
                }

                gcHandle.Free();
            }
            else
            {
                if (_classPtr == IntPtr.Zero)
                    throw new Exception("Class pointer is null - object is not correctly initialized");

                if (_nativeAddress == IntPtr.Zero)
                    throw new Exception("Native address is null - object is not correctly initialized");

                HypObject_IncRef(_classPtr, _nativeAddress, false);
            }
            
            Logger.Log(LogType.Debug, "Created HypObject of type " + GetType().Name + ", _classPtr: " + _classPtr + ", _nativeAddress: " + _nativeAddress);
        }

        ~HypObject()
        {
            Logger.Log(LogType.Debug, "Destroying HypObject of type " + GetType().Name + ", _classPtr: " + _classPtr + ", _nativeAddress: " + _nativeAddress);

            if (IsValid && Class.IsReferenceCounted)
                HypObject_DecRef(_classPtr, _nativeAddress, false);
        }

        public void Dispose()
        {
            if (IsValid)
            {
                if (Class.IsReferenceCounted)
                {
#if DEBUG
                    Assert.Throw(HypObject_GetRefCountStrong(_classPtr, _nativeAddress) == 1, "Strong reference must be 1 before destruction");
#endif

                    HypObject_DecRef(_classPtr, _nativeAddress, false);

                    Logger.Log(LogType.Debug, "Disposed HypObject of type " + GetType().Name + ", _classPtr: " + _classPtr + ", _nativeAddress: " + _nativeAddress);
                }

                GC.SuppressFinalize(this);
            }

            _classPtr = IntPtr.Zero;
            _nativeAddress = IntPtr.Zero;
        }

        public bool IsValid
        {
            get
            {
                return _classPtr != IntPtr.Zero
                    && _nativeAddress != IntPtr.Zero;
            }
        }

        public Class Class
        {
            get
            {
                return new Class(_classPtr);
            }
        }

        public IntPtr NativeAddress
        {
            get
            {
                return _nativeAddress;
            }
        }

        public Property GetProperty(Name name)
        {
            if (_classPtr == IntPtr.Zero)
            {
                throw new Exception("Class pointer is null");
            }

            IntPtr propertyPtr = Class_GetProperty(_classPtr, ref name);

            if (propertyPtr == IntPtr.Zero)
            {
                string propertiesString = "";

                foreach (Property property in Class.Properties)
                {
                    propertiesString += property.Name + ", ";
                }

                throw new Exception("Failed to get property \"" + name + "\" from Class \"" + Class.Name + "\". Available properties: " + propertiesString);
            }

            return new Property(propertyPtr);
        }

        public Method GetMethod(Name name)
        {
            if (_classPtr == IntPtr.Zero)
            {
                throw new Exception("Class pointer is null");
            }

            IntPtr methodPtr = Class_GetMethod(_classPtr, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get method \"" + name + "\" from Class \"" + Class.Name + "\"");
            }

            return new Method(methodPtr);
        }

        public static Method GetMethod(Class cls, Name name)
        {
            IntPtr methodPtr = Class_GetMethod(cls.Address, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get method \"" + name + "\" from Class \"" + cls.Name + "\"");
            }

            return new Method(methodPtr);
        }

        public T? InvokeNativeMethod<T>(Name name, object[]? args = null)
        {
            if (_classPtr == IntPtr.Zero)
            {
                throw new Exception("Class pointer is null");
            }

            Method method = GetMethod(name);

            using (HypDataBuffer resultData = method.InvokeNativeWithThis(this, args))
            {
                return (T?)resultData.GetValue();
            }
        }

        public void InvokeNativeMethod(Name name, object[]? args = null)
        {
            if (_classPtr == IntPtr.Zero)
            {
                throw new Exception("Class pointer is null");
            }

            Logger.Log(LogType.Debug, $"Invoking native method {name} on {this}");

            Method method = GetMethod(name);

            using (HypDataBuffer resultData = method.InvokeNativeWithThis(this, args))
            {
                // do nothing with the result
            }
        }

        public object? ReadNativeField(Name name)
        {
            if (_classPtr == IntPtr.Zero)
            {
                throw new Exception("Class pointer is null");
            }

            Field? field = Class.GetField(name);

            if (field == null)
            {
                throw new Exception($"Field '{name}' not found in Class '{Class.Name}'");
            }

            return ((Field)field).ReadObject(this);
        }

        public uint RefCount
        {
            get
            {
                if (_classPtr == IntPtr.Zero)
                {
                    throw new Exception("Class pointer is null");
                }

                if (_nativeAddress == IntPtr.Zero)
                {
                    throw new Exception("Native address is null");
                }

                return HypObject_GetRefCountStrong(_classPtr, _nativeAddress);
            }
        }

        public override string ToString()
        {
            return $"[HypObject: {Class.Name}, Address: 0x{(long)NativeAddress:X}]";
        }
        
        [DllImport("hyperion", EntryPoint = "HypObject_Initialize")]
        private static extern void HypObject_Initialize([In] IntPtr classPtr, [In] IntPtr pClass, [In] ref ObjectReference objectReference, [Out] out IntPtr outInstancePtr);

        [DllImport("hyperion", EntryPoint = "HypObject_GetRefCountStrong")]
        private static extern uint HypObject_GetRefCountStrong([In] IntPtr classPtr, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "HypObject_IncRef")]
        private static extern void HypObject_IncRef([In] IntPtr classPtr, [In] IntPtr nativeAddress, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "HypObject_DecRef")]
        private static extern void HypObject_DecRef([In] IntPtr classPtr, [In] IntPtr nativeAddress, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "Class_GetProperty")]
        private static extern IntPtr Class_GetProperty([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "Class_GetMethod")]
        private static extern IntPtr Class_GetMethod([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "NativeInterop_AddObjectToCache")]
        private static extern void NativeInterop_AddObjectToCache([In] IntPtr objectWrapperPtr, [Out] out IntPtr outClassObjectPtr, [Out] IntPtr outObjectReferencePtr, [MarshalAs(UnmanagedType.I1)] bool isWeak);
    }
}