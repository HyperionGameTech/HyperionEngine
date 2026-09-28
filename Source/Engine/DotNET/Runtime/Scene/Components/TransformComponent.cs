using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="TransformComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 48, Pack = 16)]
    public ref struct TransformComponent : IComponent
    {
        [FieldOffset(0)]
        public Vec3f Translation = Vec3f.Zero;

        [FieldOffset(16)]
        public Quat4f Rotation = Quat4f.Identity;

        [FieldOffset(32)]
        public Vec3f Scale = Vec3f.One;

        public TransformComponent()
        {
        }
    }
}