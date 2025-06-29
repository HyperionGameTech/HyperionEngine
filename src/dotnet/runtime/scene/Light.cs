using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "LightType")]
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

        ~Light()
        {
            Logger.Log(LogType.Warn, "Destroying Light with Id: {0}", this.Id);
        }
    }
}