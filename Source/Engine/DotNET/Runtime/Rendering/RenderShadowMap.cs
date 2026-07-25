using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="ShadowMapType")]
    public enum ShadowMapType : byte
    {
        Directional = 0,
        Spot,
        Omni,
        Count
    }
}