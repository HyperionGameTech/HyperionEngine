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

        public Handle<CharacterController> CharacterController;
        public float MoveSpeed;
        public Vec3f ViewDirection;
        public Handle<InputHandlerBase> InputHandler;

        public void Dispose()
        {
            CharacterController.Dispose();
            InputHandler.Dispose();
        }
    }
}