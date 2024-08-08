using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class HypObject
    {
        private IntPtr nativeAddress = IntPtr.Zero;
        private HypClass hypClass = HypClass.Invalid;

        private void InitializeHypObject(IntPtr nativeAddress, IntPtr hypClassPtr)
        {
            this.nativeAddress = nativeAddress;
            this.hypClass = new HypClass(hypClassPtr);
        }

        public bool IsValid
        {
            get
            {
                return nativeAddress != IntPtr.Zero
                    && hypClass.Address != IntPtr.Zero;
            }
        }

        public IntPtr NativeAddress
        {
            get
            {
                return nativeAddress;
            }
        }

        public HypClass HypClass
        {
            get
            {
                return hypClass;
            }
        }
    }
}