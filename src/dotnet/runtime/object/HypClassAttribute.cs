using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypClassAttribute
    {
        private IntPtr ptr;

        internal HypClassAttribute(IntPtr ptr)
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

        public string Name
        {
            get
            {
                if (!IsValid)
                {
                    return string.Empty;
                }

                return Marshal.PtrToStringAnsi(HypClassAttribute_GetName(ptr));
            }
        }

        public string GetString()
        {
            if (!IsValid)
            {
                return string.Empty;
            }

            return Marshal.PtrToStringAnsi(HypClassAttribute_GetString(ptr));
        }

        public bool GetBool()
        {
            if (!IsValid)
            {
                return false;
            }

            return HypClassAttribute_GetBool(ptr);
        }

        public int GetInt()
        {
            if (!IsValid)
            {
                return 0;
            }

            return HypClassAttribute_GetInt(ptr);
        }
        
        [DllImport("hyperion", EntryPoint = "HypClassAttribute_GetName")]
        private static extern IntPtr HypClassAttribute_GetName([In] IntPtr hypClassAttributePtr);

        [DllImport("hyperion", EntryPoint = "HypClassAttribute_GetString")]
        private static extern IntPtr HypClassAttribute_GetString([In] IntPtr hypClassAttributePtr);

        [DllImport("hyperion", EntryPoint = "HypClassAttribute_GetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypClassAttribute_GetBool([In] IntPtr hypClassAttributePtr);

        [DllImport("hyperion", EntryPoint = "HypClassAttribute_GetInt")]
        private static extern int HypClassAttribute_GetInt([In] IntPtr hypClassAttributePtr);
    }
}