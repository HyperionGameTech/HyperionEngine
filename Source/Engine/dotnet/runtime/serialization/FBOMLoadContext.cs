using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class FBOMLoadContext
    {

        internal IntPtr ptr;

        public FBOMLoadContext()
        {
        }

        internal FBOMLoadContext(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        internal IntPtr Address
        {
            get
            {
                return ptr;
            }
        }
    }
}