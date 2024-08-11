using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypProperty
    {
        public static readonly HypProperty Invalid = new HypProperty(IntPtr.Zero);

        internal IntPtr ptr;

        internal HypProperty(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypProperty_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypProperty_GetTypeID(ptr, out typeId);
                return typeId;
            }
        }
        
        public HypData InvokeGetter(HypObject hypObject)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid property");
            }

            if (!hypObject.IsValid)
            {
                throw new InvalidOperationException("Cannot invoke getter: Invalid target object");
            }

            HypData result;

            if (!HypProperty_InvokeGetter(ptr, hypObject.HypClass.Address, hypObject.NativeAddress, out result))
            {
                throw new InvalidOperationException("Failed to invoke getter");
            }

            return result;
        }
        
        [DllImport("hyperion", EntryPoint = "HypProperty_GetName")]
        private static extern void HypProperty_GetName([In] IntPtr propertyPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypProperty_GetTypeID")]
        private static extern void HypProperty_GetTypeID([In] IntPtr propertyPtr, [Out] out TypeID typeId);

        [DllImport("hyperion", EntryPoint = "HypProperty_InvokeGetter")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypProperty_InvokeGetter([In] IntPtr propertyPtr, [In] IntPtr targetClassPtr, [In] IntPtr targetPtr, [Out] out HypData outResult);
    }

    public struct HypClass
    {
        public static readonly HypClass Invalid = new HypClass(IntPtr.Zero);

        private IntPtr ptr;

        internal HypClass(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        internal IntPtr Address
        {
            get
            {
                return ptr;
            }
            set
            {
                ptr = value;
            }
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypClass_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypClass_GetTypeID(ptr, out typeId);
                return typeId;
            }
        }

        public IEnumerable<HypProperty> Properties
        {
            get
            {
                IntPtr propertiesPtr;
                uint count = HypClass_GetProperties(ptr, out propertiesPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr propertyPtr = Marshal.ReadIntPtr(propertiesPtr, i * IntPtr.Size);
                    yield return new HypProperty(propertyPtr);
                }
            }
        }

        public HypProperty? GetProperty(Name name)
        {
            IntPtr propertyPtr = HypClass_GetProperty(ptr, ref name);

            if (propertyPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypProperty(propertyPtr);
        }

        /// <summary>
        ///  Creates a native instance of this object. The object has an initial ref count of zero,
        ///  so the reference must be manually decremented to destroy the object (with HypObject_DecRef)  
        /// </summary>
        public IntPtr CreateInstance()
        {
            IntPtr instancePtr = HypClass_CreateInstance(ptr);

            if (instancePtr == IntPtr.Zero)
            {
                throw new Exception("Failed to initialize HypObject");
            }

            return instancePtr;
        }

        public static bool operator==(HypClass a, HypClass b)
        {
            return a.ptr == b.ptr;
        }

        public static bool operator!=(HypClass a, HypClass b)
        {
            return a.ptr != b.ptr;
        }

        public static HypClass? GetClass(string name)
        {
            IntPtr ptr = HypClass_GetClassByName(name);

            if (ptr == IntPtr.Zero)
            {
                return null;
            }

            return new HypClass(ptr);
        }
        
        [DllImport("hyperion", EntryPoint = "HypClass_CreateInstance")]
        private static extern IntPtr HypClass_CreateInstance([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetName")]
        private static extern void HypClass_GetName([In] IntPtr hypClassPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetTypeID")]
        private static extern void HypClass_GetTypeID([In] IntPtr hypClassPtr, [Out] out TypeID typeId);

        [DllImport("hyperion", EntryPoint = "HypClass_GetProperties")]
        private static extern uint HypClass_GetProperties([In] IntPtr hypClassPtr, [Out] out IntPtr outPropertiesPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetProperty")]
        private static extern IntPtr HypClass_GetProperty([In] IntPtr hypClassPtr, [In] ref Name name);
    }
}