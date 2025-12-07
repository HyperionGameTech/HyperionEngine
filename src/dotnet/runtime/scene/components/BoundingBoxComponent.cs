using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="BoundingBoxComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public struct BoundingBoxComponent : IComponent
    {
        private BoundingBox _worldAabb;

        public void Dispose()
        {
        }

        public BoundingBox WorldAABB
        {
            get => _worldAabb;
            set => _worldAabb = value;
        }
    }
}