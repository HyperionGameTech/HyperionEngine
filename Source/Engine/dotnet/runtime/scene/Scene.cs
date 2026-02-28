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

    [ClassBinding(Name = "FogParams")]
    public struct FogParams
    {
        public Color Color;
        public float StartDistance;
        public float EndDistance;
    }

    [ClassBinding(Name = "CSMParams")]
    public struct CSMParams
    {
        public uint NumCascades;
    }

    [ClassBinding(Name = "CSMState")]
    public struct CSMState
    {
        public Vec3f PlayerCenter;
    }

    [ClassBinding(Name = "Scene")]
    public class Scene : AssetObject
    {
        public Scene()
        {
        }

        public Node RootNode
        {
            get
            {
                return this.GetRoot();
            }
        }
    }
}