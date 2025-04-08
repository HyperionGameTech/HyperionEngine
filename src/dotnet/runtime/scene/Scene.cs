using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="SceneFlags")]
    [Flags]
    public enum SceneFlags : uint
    {
        None = 0,
        Foreground = 0x1,
        Detached = 0x2,
        UI = 0x8
    }

    [HypClassBinding(Name="Scene")]
    public class Scene : HypObject
    {
        public Scene()
        {
        }
    }
}