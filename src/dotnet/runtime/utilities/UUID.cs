using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="UUID")]
    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct UUID
    {
        private ulong data0;
        private ulong data1;

        public UUID()
        {
            data0 = 0;
            data1 = 0;
        }

        public UUID(ulong data0, ulong data1)
        {
            this.data0 = data0;
            this.data1 = data1;
        }
        
        public Guid ToGuid()
        {
            return new Guid((uint)data0, (ushort)(data0 >> 32), (ushort)(data0 >> 48), (byte)data1, (byte)(data1 >> 8), (byte)(data1 >> 16), (byte)(data1 >> 24), (byte)(data1 >> 32), (byte)(data1 >> 40), (byte)(data1 >> 48), (byte)(data1 >> 56));
        }
    }
}