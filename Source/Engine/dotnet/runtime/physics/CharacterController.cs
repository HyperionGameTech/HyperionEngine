using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "CharacterControllerInputHandler")]
    public abstract class CharacterControllerInputHandler : InputHandlerBase
    {
        public CharacterControllerInputHandler()
        {
        }
    }

    [ClassBinding(Name = "CharacterController")]
    public abstract class CharacterController : ObjectBase
    {
        public CharacterController()
        {
        }
    }
}