using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    /// <summary>
    /// Represents a C++ system in the scene.
    /// </summary>
    [ClassBinding(Name = "SystemBase")]
    public class SystemBase : ObjectBase
    {
        internal SystemBase()
        {
        }
    }
}
