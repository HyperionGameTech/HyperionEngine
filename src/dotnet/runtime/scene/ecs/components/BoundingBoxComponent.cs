using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="BoundingBoxComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 64)]
    public struct BoundingBoxComponent : IComponent
    {
        [FieldOffset(0)]
        private BoundingBox localAABB;

        [FieldOffset(32)]
        private BoundingBox worldAABB;

        public void Dispose()
        {
        }

        public BoundingBox LocalAABB
        {
            get
            {
                return localAABB;
            }
            set
            {
                localAABB = value;
            }
        }

        public BoundingBox WorldAABB
        {
            get
            {
                return worldAABB;
            }
            set
            {
                worldAABB = value;
            }
        }
    }
}