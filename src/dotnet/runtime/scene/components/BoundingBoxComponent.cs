using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="BoundingBoxComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public ref struct BoundingBoxComponent : IComponent
    {
        public BoundingBox WorldAABB;

        public void Dispose()
        {
        }
        
        public static Class Class => Class.GetClass(typeof(BoundingBoxComponent));
    }
}