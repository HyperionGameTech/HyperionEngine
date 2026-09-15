using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="CharacterMovementSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterMovementSettings
    {
        public float MoveSpeed;
        public float SprintSpeed;
        public float GroundAcceleration;
        public float AirAcceleration;
        public float Friction;
        public float StopSpeed;
        public float StepHeight;
        public float MaxSlopeAngle;
    }

    [ClassBinding(Name="CharacterJumpSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterJumpSettings
    {
        public float Speed;
        public float CutGravityMultiplier;
        public float ApexGravityMultiplier;
        public float FallGravityMultiplier;
        public float FallSpeed;
        public float CoyoteTime;
        public float BufferTime;
    }

    [ClassBinding(Name="CharacterPushSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterPushSettings
    {
        public float MassLimit;
        public float MaxSpeed;
        public float SpeedScale;
        public float PredictionReleaseDelay;
        public float MinGroundSupportMass;
    }

    [ClassBinding(Name="CharacterShadowBodySettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterShadowBodySettings
    {
        public float MaxSpeed;
        public float TeleportDistance;
    }

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

        public CharacterMovementSettings Movement;
        public CharacterJumpSettings Jump;
        public CharacterPushSettings Push;
        public CharacterShadowBodySettings ShadowBody;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsOnGround;

        public void Dispose()
        {
            Shape.Dispose();
            InputHandler.Dispose();
        }
    }
}
