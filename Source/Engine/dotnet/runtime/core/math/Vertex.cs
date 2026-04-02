using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="VertexTypeMask")]
    [StructLayout(LayoutKind.Sequential)]
    public struct VertexTypeMask
    {
        public byte flagMask;

        public VertexTypeMask()
        {
            flagMask = 0;
        }

        public VertexTypeMask(byte flagMask)
        {
            this.flagMask = flagMask;
        }
    }

    [ClassBinding(Name = "VertexInputLayoutDesc")]
    [StructLayout(LayoutKind.Sequential)]
    public struct VertexInputLayoutDesc
    {
        public byte mask;
    }
}
