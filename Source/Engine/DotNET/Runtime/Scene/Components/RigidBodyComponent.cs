using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="RigidBodyComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 64)]
    public ref struct RigidBodyComponent : IComponent
    {
        [FieldOffset(0)]
        public PhysicsMaterial PhysicsMaterial;

        [FieldOffset(16)]
        public Vec3f initialVelocity;

        [FieldOffset(32)]
        public Vec3f initialAngularVelocity;

        [FieldOffset(48)]
        public Handle<PhysicsShape> Shape;

        [FieldOffset(56)]
        public Handle<RigidBody> RigidBody;

        public void Dispose()
        {
            Shape.Dispose();
            RigidBody.Dispose();
        }
    }
}