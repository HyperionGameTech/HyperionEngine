using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "InputHandlerBase")]
    public abstract class InputHandlerBase : ObjectBase
    {
        public InputHandlerBase()
        {
        }
    }

    [ClassBinding(Name = "FirstPersonCameraInputHandler")]
    public class FirstPersonCameraInputHandler : InputHandlerBase
    {
        public FirstPersonCameraInputHandler()
        {
        }
    }

    [ClassBinding(Name = "CharacterControllerInputHandler")]
    public class CharacterControllerInputHandler : InputHandlerBase
    {
        public CharacterControllerInputHandler()
        {
        }
    }

#if HYP_EDITOR
    [ClassBinding(Name = "EditorCameraInputHandler")]
    public class EditorCameraInputHandler : InputHandlerBase
    {
        public EditorCameraInputHandler()
        {
        }
    }
#endif
}
