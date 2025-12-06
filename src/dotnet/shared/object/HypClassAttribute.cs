using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct ClassAttribute
    {
        private IntPtr ptr;

        internal ClassAttribute(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Name Name
        {
            get
            {
                if (!IsValid)
                {
                    return Name.Invalid;
                }

                if (ClassAttribute_GetName(ptr, out Name name))
                    return name;

                return Name.Invalid;
            }
        }

        public string GetString()
        {
            if (!IsValid)
            {
                return string.Empty;
            }

            return Marshal.PtrToStringAnsi(ClassAttribute_GetString(ptr)) ?? string.Empty;
        }

        public bool GetBool()
        {
            if (!IsValid)
            {
                return false;
            }

            return ClassAttribute_GetBool(ptr);
        }

        public int GetInt()
        {
            if (!IsValid)
            {
                return 0;
            }

            return ClassAttribute_GetInt(ptr);
        }
        
        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetName")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ClassAttribute_GetName([In] IntPtr classAttributePtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetString")]
        private static extern IntPtr ClassAttribute_GetString([In] IntPtr classAttributePtr);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ClassAttribute_GetBool([In] IntPtr classAttributePtr);

        [DllImport("hyperion", EntryPoint = "ClassAttribute_GetInt")]
        private static extern int ClassAttribute_GetInt([In] IntPtr classAttributePtr);
    }
}