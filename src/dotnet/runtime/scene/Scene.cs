using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "SceneFlags")]
    [Flags]
    public enum SceneFlags : uint
    {
        None = 0,
        Foreground = 0x1,
        Detached = 0x2,
        UI = 0x8
    }

    [ClassBinding(Name = "Scene")]
    public class Scene : ObjectBase
    {
        public Scene()
        {
        }
    }
}