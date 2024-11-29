using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public class HypObject : IDisposable
    {
        public IntPtr _hypClassPtr;
        public IntPtr _nativeAddress;
        private IntPtr controlBlockPtr;
        private bool isWeak;

        protected HypObject()
        {
            bool initiatedFromManagedSide = _nativeAddress == IntPtr.Zero;

            // If initiated from managed side, this is a new object and we retain a strong reference to it.
            // otherwise, we retain a weak reference to it
            isWeak = !initiatedFromManagedSide;

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

                HypClass hypClass = attribute.HypClass;

                if (hypClass == HypClass.Invalid)
                {
                    throw new Exception("Invalid HypClass returned from HypClassBinding attribute");
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

                    NativeInterop_AddObjectToCache(objectWrapperPtr, out classObjectPtr, objectReferencePtr);

#if DEBUG
                    if (!objectReference.IsValid)
                    {
                        throw new Exception("Failed to add object to cache");
                    }
#endif

                    _hypClassPtr = hypClass.Address;
                    
                    HypObject_Initialize(_hypClassPtr, classObjectPtr, ref objectReference, out _nativeAddress);

#if DEBUG
                    HypObject_Verify(_hypClassPtr, _nativeAddress, objectReferencePtr);
#endif
                }

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

            controlBlockPtr = HypObject_IncRef(_hypClassPtr, _nativeAddress, isWeak);

            Logger.Log(LogType.Debug, "Created HypObject of type " + GetType().Name + ", _hypClassPtr: " + _hypClassPtr + ", _nativeAddress: " + _nativeAddress);
        }

        ~HypObject()
        {
            Logger.Log(LogType.Debug, "Destroying HypObject of type " + GetType().Name + ", _hypClassPtr: " + _hypClassPtr + ", _nativeAddress: " + _nativeAddress);

            if (IsValid)
            {
                HypObject_DecRef(_hypClassPtr, _nativeAddress, controlBlockPtr, isWeak);

                _hypClassPtr = IntPtr.Zero;
                _nativeAddress = IntPtr.Zero;
            }
        }

        public void Dispose()
        {
            if (IsValid)
            {
                HypObject_DecRef(_hypClassPtr, _nativeAddress, controlBlockPtr, isWeak);

                _hypClassPtr = IntPtr.Zero;
                _nativeAddress = IntPtr.Zero;

                GC.SuppressFinalize(this);
            }
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

        public IntPtr ControlBlockPtr
        {
            get
            {
                return controlBlockPtr;
            }
        }

        public HypProperty GetProperty(Name name)
        {
            if (_hypClassPtr == IntPtr.Zero)
            {
                throw new Exception("HypClass pointer is null");
            }

            IntPtr propertyPtr = HypObject_GetProperty(_hypClassPtr, ref name);

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

            IntPtr methodPtr = HypObject_GetMethod(_hypClassPtr, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get method \"" + name + "\" from HypClass \"" + HypClass.Name + "\"");
            }

            return new HypMethod(methodPtr);
        }

        public static HypMethod GetMethod(HypClass hypClass, Name name)
        {
            IntPtr methodPtr = HypObject_GetMethod(hypClass.Address, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get method \"" + name + "\" from HypClass \"" + hypClass.Name + "\"");
            }

            return new HypMethod(methodPtr);
        }
        
        [DllImport("hyperion", EntryPoint = "HypObject_Initialize")]
        private static extern void HypObject_Initialize([In] IntPtr hypClassPtr, [In] IntPtr classObjectPtr, [In] ref ObjectReference objectReference, [Out] out IntPtr outInstancePtr);

        [DllImport("hyperion", EntryPoint = "HypObject_Verify")]
        private static extern void HypObject_Verify([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress, [In] IntPtr objectReferencePtr);

        [DllImport("hyperion", EntryPoint = "HypObject_IncRef")]
        private static extern IntPtr HypObject_IncRef([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "HypObject_DecRef")]
        private static extern void HypObject_DecRef([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress, [In] IntPtr controlBlockPtr, [MarshalAs(UnmanagedType.I1)] bool isWeak);

        [DllImport("hyperion", EntryPoint = "HypObject_GetProperty")]
        private static extern IntPtr HypObject_GetProperty([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "HypObject_GetMethod")]
        private static extern IntPtr HypObject_GetMethod([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "NativeInterop_AddObjectToCache")]
        private static extern void NativeInterop_AddObjectToCache([In] IntPtr objectWrapperPtr, [Out] out IntPtr outClassObjectPtr, [Out] IntPtr outObjectReferencePtr);
    }
}