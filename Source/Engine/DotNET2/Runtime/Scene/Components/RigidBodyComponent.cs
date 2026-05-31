using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="RigidBodyComponent")]
    [StructLayout(LayoutKind.Sequential)]
    public ref struct RigidBodyComponent : IComponent
    {
        public static Class Class => Class.GetClass(typeof(RigidBodyComponent));

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (RigidBodyComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }

        public Handle<RigidBody> RigidBody;
        public PhysicsMaterial PhysicsMaterial;

        public void Dispose()
        {
            RigidBody.Dispose();
        }
    }
}