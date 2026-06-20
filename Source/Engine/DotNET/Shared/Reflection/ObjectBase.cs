using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Collections.Concurrent;

namespace Hyperion
{
    public class ObjectBase : IDisposable
    {
        public IntPtr _classPtr;
        public IntPtr _nativeAddress;

        private static readonly ReaderWriterLockSlim _shutdownLock = new(LockRecursionPolicy.SupportsRecursion);
        private static int _isEngineShuttingDown = 0;

        public static bool IsEngineShuttingDown
        {
            get
            {
                _shutdownLock.EnterReadLock();

                bool value = _isEngineShuttingDown != 0;
                
                _shutdownLock.ExitReadLock();

                return value;
            }
            set
            {
                _shutdownLock.EnterWriteLock();

                Interlocked.Exchange(ref _isEngineShuttingDown, value ? 1 : 0);

                _shutdownLock.ExitWriteLock();
            }
        }

        protected ObjectBase()
        {
            _shutdownLock.EnterReadLock();

            try
            {
                if (_isEngineShuttingDown != 0)
                {
                    throw new Exception("Cannot create ObjectBase instance after engine is shutting down");
                }

                bool initiatedFromManagedSide = _nativeAddress == IntPtr.Zero;

                if (initiatedFromManagedSide)
                {
                    Type type = GetType();

                    Class cls = Class.GetClass(type);

                    if (cls == Class.Invalid)
                    {
                        throw new Exception("Invalid Class returned from ClassBinding attribute");
                    }

                    if (!cls.IsReferenceCounted)
                    {
                        throw new Exception("Can only create instances of reference counted Class objects (using Handle<T>) from managed code");
                    }

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

                        Object_Initialize(_classPtr, pClass, ref objectReference, out _nativeAddress);
                    }

                    gcHandle.Free();
                }
                else
                {
                    if (_classPtr == IntPtr.Zero)
                        throw new Exception("Class pointer is null - object is not correctly initialized");

                    if (_nativeAddress == IntPtr.Zero)
                        throw new Exception("Native address is null - object is not correctly initialized");

                    Object_IncRef(_classPtr, _nativeAddress, false);
                }

                Logger.Log(LogLevel.Verbose, "Construct ObjectBase of type " + GetType().Name + ", _classPtr: " + _classPtr + ", _nativeAddress: " + _nativeAddress);
            }
            finally
            {
                _shutdownLock.ExitReadLock();
            }
        }

        ~ObjectBase()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);

            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool isDisposing)
        {
            if (IsValid)
            {
                _shutdownLock.EnterReadLock();

                try
                {
                    if (Class.IsReferenceCounted && _isEngineShuttingDown == 0)
                    {
#if DEBUG
                        Assert.Throw(Object_GetRefCountStrong(_classPtr, _nativeAddress) == 1, "Strong reference must be 1 before destruction");
#endif

                        Object_DecRef(_classPtr, _nativeAddress, false);

                        Logger.Log(LogLevel.Verbose, "Disposed ObjectBase of type " + GetType().Name + ", _classPtr: " + _classPtr + ", _nativeAddress: " + _nativeAddress);
                    }
                }
                finally
                {
                    _shutdownLock.ExitReadLock();
                }
            }

            _classPtr = IntPtr.Zero;
            _nativeAddress = IntPtr.Zero;
        }

        public ObjIdBase Id
        {
            get
            {
                if (_nativeAddress == IntPtr.Zero)
                {
                    throw new Exception("Native address is null - cannot get Id");
                }

                Object_GetId(_nativeAddress, out ObjIdBase id);
                return id;
            }
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

            using BoxedValueInternal resultData = method.InvokeNativeWithThis(this, args);
            return (T?)resultData.GetValue();
        }

        public void InvokeNativeMethod(Name name, object[]? args = null)
        {
            if (_classPtr == IntPtr.Zero)
            {
                throw new Exception("Class pointer is null");
            }

            Method method = GetMethod(name);

            using BoxedValueInternal resultData = method.InvokeNativeWithThis(this, args);
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

                return Object_GetRefCountStrong(_classPtr, _nativeAddress);
            }
        }

        public override string ToString()
        {
            return $"[ObjectBase: {Class.Name}, Address: 0x{(long)NativeAddress:X}]";
        }

        public override bool Equals(object? obj)
        {
            if (obj is ObjectBase other)
            {
                return _nativeAddress == other._nativeAddress && _nativeAddress != IntPtr.Zero;
            }

            return false;
        }

        public override int GetHashCode()
        {
            return _nativeAddress.GetHashCode();
        }

        public static bool operator ==(ObjectBase? left, ObjectBase? right)
        {
            if (ReferenceEquals(left, right))
                return true;

            if (left is null || right is null)
                return false;

            return left.Equals(right);
        }

        public static bool operator !=(ObjectBase? left, ObjectBase? right)
        {
            return !(left == right);
        }

        [DllImport("hyperion", EntryPoint = "Object_Initialize")]
        private static extern void Object_Initialize([In] IntPtr classPtr, [In] IntPtr pClass, [In] ref ObjectReference objectReference, [Out] out IntPtr outInstancePtr);

        [DllImport("hyperion", EntryPoint = "Object_GetId")]
        private static extern void Object_GetId([In] IntPtr nativeAddress, [Out] out ObjIdBase outIdValue);

        [DllImport("hyperion", EntryPoint = "Object_GetRefCountStrong")]
        private static extern uint Object_GetRefCountStrong([In] IntPtr classPtr, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "Object_IncRef")]
        private static extern void Object_IncRef([In] IntPtr classPtr, [In] IntPtr nativeAddress, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "Object_DecRef")]
        private static extern void Object_DecRef([In] IntPtr classPtr, [In] IntPtr nativeAddress, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "Class_GetProperty")]
        private static extern IntPtr Class_GetProperty([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "Class_GetMethod")]
        private static extern IntPtr Class_GetMethod([In] IntPtr classPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "NativeInterop_AddObjectToCache")]
        private static extern void NativeInterop_AddObjectToCache([In] IntPtr objectWrapperPtr, [Out] out IntPtr outClassObjectPtr, [Out] IntPtr outObjectReferencePtr, [MarshalAs(UnmanagedType.I1)] bool isWeak);
    }
}
