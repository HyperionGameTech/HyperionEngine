using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Camera")]
    public class Camera : HypObject
    {
        public Camera()
        {
        }

        public IDBase ID
        {
            get
            {
                return (IDBase)GetProperty(PropertyNames.ID)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }
    }
}