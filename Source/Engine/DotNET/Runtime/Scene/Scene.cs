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
        Backdrop = 0x2,
        Detached = 0x4,
        UI = 0x8,
        Editor = 0x10,

        Streamed = 0x20,
        HasOctree = 0x40,

        AudioListener = 0x80,

        Default = Foreground | Streamed | HasOctree
    }

    [ClassBinding(Name = "Scene")]
    public class Scene : AssetObject
    {
        public Scene()
        {
        }

        protected override void Dispose(bool isDisposing)
        {
            if (isDisposing)
            {
                RootNode?.Dispose();
            }

            base.Dispose(isDisposing);
        }

        public Node? RootNode => this.GetRoot();

        public World? World => this.GetWorld();

        public SceneFlags SceneFlags
        {
            get => this.GetSceneFlags();
            set => this.SetSceneFlags(value);
        }
    }
}