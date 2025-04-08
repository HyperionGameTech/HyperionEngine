using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Collections.Concurrent;

namespace Hyperion
{
    public class HypObject : IDisposable
    {
        public IntPtr _hypClassPtr;
        public IntPtr _nativeAddress;

        protected HypObject()
        {
            bool initiatedFromManagedSide = _nativeAddress == IntPtr.Zero;

            if (initiatedFromManagedSide)
            {
                Type type = this.GetType();

                // Read the HypClassBinding attribute
                // allow inheritance, so we can get the closest HypClassBinding attribute, allowing
                // for other HypObject types that bind to native C++ classes to be overridden.
                HypClassBinding? attribute = HypClassBinding.ForType(type, inheritance: true);

                if (attribute == null)
                {
                    throw new Exception("Failed to get HypClassBinding attribute for type " + type.Name);
                }

                HypClass hypClass = attribute.GetClass(type);

                if (hypClass == HypClass.Invalid)
                {
                    throw new Exception("Invalid HypClass returned from HypClassBinding attribute");
                }

                if (!hypClass.IsReferenceCounted)
                {
                    throw new Exception("Can only create instances of reference counted HypClass objects (using RC<T> or Handle<T>) from managed code");
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

                    IntPtr classObjectPtr = IntPtr.Zero;

                    NativeInterop_AddObjectToCache(objectWrapperPtr, out classObjectPtr, objectReferencePtr, isWeak: true);

#if DEBUG
                    if (!objectReference.IsValid)
                    {
                        Console.WriteLine("Failed to add object to cache");
                        Console.Out.Flush();

                        gcHandle.Free();
                        throw new Exception("Failed to add object to cache");
                    }
#endif

                    _hypClassPtr = hypClass.Address;
                    
                    HypObject_Initialize(_hypClassPtr, classObjectPtr, ref objectReference, out _nativeAddress);

#if DEBUG
                    HypObject_Verify(_hypClassPtr, _nativeAddress, objectReferencePtr);
#endif
                }

                Console.WriteLine("Created HypObject of type " + GetType().Name + ", _hypClassPtr: " + _hypClassPtr + ", _nativeAddress: " + _nativeAddress);
                Console.Out.Flush();

                gcHandle.Free();
            }
            else
            {
                if (_hypClassPtr == IntPtr.Zero)
                {
                    throw new Exception("HypClass pointer is null - object is not correctly initialized");
                }

                if (_nativeAddress == IntPtr.Zero)
                {
                    throw new Exception("Native address is null - object is not correctly initialized");
                }

#if DEBUG
                HypObject_Verify(_hypClassPtr, _nativeAddress, IntPtr.Zero);
#endif
            }
            
            // Logger.Log(LogType.Debug, "Created HypObject of type " + GetType().Name + ", _hypClassPtr: " + _hypClassPtr + ", _nativeAddress: " + _nativeAddress);
        }

        ~HypObject()
        {
            // Logger.Log(LogType.Debug, "Destroying HypObject of type " + GetType().Name + ", _hypClassPtr: " + _hypClassPtr + ", _nativeAddress: " + _nativeAddress);

            if (IsValid)
            {
                if (HypClass.IsReferenceCounted)
                {
#if DEBUG
                    Assert.Throw(HypObject_GetRefCount_Strong(_hypClassPtr, _nativeAddress) > 0, "Strong reference must be > 0 before destruction");
#endif

                    HypObject_DecRef(_hypClassPtr, _nativeAddress, false);
                }
            }
        }

        public void Dispose()
        {
            if (IsValid)
            {
                if (HypClass.IsReferenceCounted)
                {
#if DEBUG
                    Assert.Throw(HypObject_GetRefCount_Strong(_hypClassPtr, _nativeAddress) > 0, "Strong reference must be > 0 before destruction");
#endif

                    HypObject_DecRef(_hypClassPtr, _nativeAddress, false);
                }

                GC.SuppressFinalize(this);
            }

            _hypClassPtr = IntPtr.Zero;
            _nativeAddress = IntPtr.Zero;
        }

        public bool IsValid
        {
            get
            {
                return _hypClassPtr != IntPtr.Zero
                    && _nativeAddress != IntPtr.Zero;
            }
        }

        public HypClass HypClass
        {
            get
            {
                return new HypClass(_hypClassPtr);
            }
        }

        public IntPtr NativeAddress
        {
            get
            {
                return _nativeAddress;
            }
        }

        public HypProperty GetProperty(Name name)
        {
            if (_hypClassPtr == IntPtr.Zero)
            {
                throw new Exception("HypClass pointer is null");
            }

            IntPtr propertyPtr = HypClass_GetProperty(_hypClassPtr, ref name);

            if (propertyPtr == IntPtr.Zero)
            {
                string propertiesString = "";

                foreach (HypProperty property in HypClass.Properties)
                {
                    propertiesString += property.Name + ", ";
                }

                throw new Exception("Failed to get property \"" + name + "\" from HypClass \"" + HypClass.Name + "\". Available properties: " + propertiesString);
            }

            return new HypProperty(propertyPtr);
        }

        public HypMethod GetMethod(Name name)
        {
            if (_hypClassPtr == IntPtr.Zero)
            {
                throw new Exception("HypClass pointer is null");
            }

            IntPtr methodPtr = HypClass_GetMethod(_hypClassPtr, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get method \"" + name + "\" from HypClass \"" + HypClass.Name + "\"");
            }

            return new HypMethod(methodPtr);
        }

        public static HypMethod GetMethod(HypClass hypClass, Name name)
        {
            IntPtr methodPtr = HypClass_GetMethod(hypClass.Address, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get method \"" + name + "\" from HypClass \"" + hypClass.Name + "\"");
            }

            return new HypMethod(methodPtr);
        }

        public override string ToString()
        {
            return $"[HypObject: {HypClass.Name}, Address: 0x{(long)NativeAddress:X}]";
        }
        
        [DllImport("hyperion", EntryPoint = "HypObject_Initialize")]
        private static extern void HypObject_Initialize([In] IntPtr hypClassPtr, [In] IntPtr classObjectPtr, [In] ref ObjectReference objectReference, [Out] out IntPtr outInstancePtr);

        [DllImport("hyperion", EntryPoint = "HypObject_Verify")]
        private static extern void HypObject_Verify([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress, [In] IntPtr objectReferencePtr);

        [DllImport("hyperion", EntryPoint = "HypObject_GetRefCount_Strong")]
        private static extern uint HypObject_GetRefCount_Strong([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "HypObject_IncRef")]
        private static extern void HypObject_IncRef([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "HypObject_DecRef")]
        private static extern void HypObject_DecRef([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "HypClass_GetProperty")]
        private static extern IntPtr HypClass_GetProperty([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetMethod")]
        private static extern IntPtr HypClass_GetMethod([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "NativeInterop_AddObjectToCache")]
        private static extern void NativeInterop_AddObjectToCache([In] IntPtr objectWrapperPtr, [Out] out IntPtr outClassObjectPtr, [Out] IntPtr outObjectReferencePtr, [MarshalAs(UnmanagedType.I1)] bool isWeak);
    }
}