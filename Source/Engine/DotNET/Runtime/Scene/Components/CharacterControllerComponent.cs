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
        public SharedPtr PhysicsHandle; // SharedPtr<void> - internal, do not access directly

        public Vec3f ViewDirection;
        public Vec3f Translation;

        public float MoveSpeed;
        public float SprintSpeed;
        public float GroundAcceleration;
        public float AirAcceleration;
        public float Friction;
        public float StopSpeed;
        public float StepHeight;
        public float MaxSlopeAngle;
        public float JumpSpeed;
        public float JumpCutGravityMultiplier;
        public float ApexGravityMultiplier;
        public float FallGravityMultiplier;
        public float FallSpeed;
        public float CoyoteTime;
        public float JumpBufferTime;
        public float ShadowMaxSpeed;
        public float ShadowTeleportDistance;
        public float PushMassLimit;
        public float MaxPushSpeed;
        public float PushSpeedScale;
        public float PushPredictionReleaseDelay;
        public float MinGroundSupportMass;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsOnGround;

        public void Dispose()
        {
            Shape.Dispose();
            InputHandler.Dispose();
        }
    }
}
