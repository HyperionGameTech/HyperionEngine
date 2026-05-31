using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="ShadowMapFilter")]
    public enum ShadowMapFilter : uint
    {
        Standard = 0,
        Pcf,
        ContactHardened,
        Vsm,

        Count
    }

    [ClassBinding(Name="ShadowMapType")]
    public enum ShadowMapType : uint
    {
        Directional = 0,
        Spot,
        Omni,
        Count
    }
}