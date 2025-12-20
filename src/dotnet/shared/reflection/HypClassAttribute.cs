using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct ClassAttribute
    {
        private IntPtr _ptr;

        internal ClassAttribute(IntPtr ptr)
        {
            _ptr = ptr;
        }

        public bool IsValid => _ptr != IntPtr.Zero;

        public Name Name
        {
            get
            {
                if (!IsValid)
                {
                    return Name.Invalid;
                }

                if (ClassAttribute_GetName(_ptr, out Name name))
                    return name;

                return Name.Invalid;
            }
        }

        public bool IsString => IsValid && ClassAttribute_IsString(_ptr);
        public bool IsBool => IsValid && ClassAttribute_IsBool(_ptr);
        public bool IsInt => IsValid && ClassAttribute_IsInt(_ptr);

        public string GetString()
        {
            if (!IsValid)
            {
                return string.Empty;
            }

            return Marshal.PtrToStringAnsi(ClassAttribute_GetString(_ptr)) ?? string.Empty;
        }

        public bool GetBool()
        {
            if (!IsValid)
            {
                return false;
            }

            return ClassAttribute_GetBool(_ptr);
        }

        public int GetInt()
        {
            if (!IsValid)
            {
                return 0;
            }

            return ClassAttribute_GetInt(_ptr);
        }
        
        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetName")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ClassAttribute_GetName([In] IntPtr classAttributePtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_IsString")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ClassAttribute_IsString([In] IntPtr classAttributePtr);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetString")]
        private static extern IntPtr ClassAttribute_GetString([In] IntPtr classAttributePtr);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_IsBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ClassAttribute_IsBool([In] IntPtr classAttributePtr);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ClassAttribute_GetBool([In] IntPtr classAttributePtr);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_IsInt")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ClassAttribute_IsInt([In] IntPtr classAttributePtr);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetInt")]
        private static extern int ClassAttribute_GetInt([In] IntPtr classAttributePtr);
    }
}