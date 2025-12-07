using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="BoundingBoxComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public struct BoundingBoxComponent : IComponent
    {
        public BoundingBox WorldAABB;

        public void Dispose()
        {
        }
    }
}