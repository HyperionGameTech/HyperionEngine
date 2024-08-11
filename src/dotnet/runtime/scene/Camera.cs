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

        public ID<Camera> ID
        {
            get
            {
                return GetProperty(PropertyNames.ID)
                    .InvokeGetter(this)
                    .GetID<Camera>();
            }
        }
    }
}