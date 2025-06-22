using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LightType : uint
    {
        Directional = 0,
        Point = 1,
        Spot = 2,
        AreaRect = 3
    }

    [HypClassBinding(Name="Light")]
    public class Light : Entity
    {
        public Light()
        {
        }
    }
}