using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="CharacterControllerComponent")]
    [StructLayout(LayoutKind.Sequential)]
    public ref struct CharacterControllerComponent : IComponent
    {
        public static Class Class => Class.GetClass(typeof(CharacterControllerComponent));

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (CharacterControllerComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }

        public Handle<PhysicsShape> Shape;
        public Handle<InputHandlerBase> InputHandler;
        public RefCountedPtr PhysicsHandle; // RC<void> - internal, do not access directly

        public Vec3f ViewDirection;
        public Vec3f Translation;

        public float MoveSpeed;
        public float StepHeight;
        public float MaxSlopeAngle;
        public float JumpSpeed;
        public float FallSpeed;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsOnGround;

        public void Dispose()
        {
            Shape.Dispose();
            InputHandler.Dispose();
        }
    }
}
