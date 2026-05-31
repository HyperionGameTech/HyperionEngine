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

    [ClassBinding(Name = "NullInputHandler")]
    public class NullInputHandler : InputHandlerBase
    {
        public NullInputHandler()
        {
        }
    }
}
