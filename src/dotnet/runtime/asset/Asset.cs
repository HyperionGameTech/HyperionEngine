using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct Asset
    {
        private IntPtr ptr;

        public Asset(IntPtr ptr)
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

        public Node GetNode()
        {
            throw new NotImplementedException();
        }

        public Texture GetTexture()
        {
            throw new NotImplementedException();
        }
    }
}