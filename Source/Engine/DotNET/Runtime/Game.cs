using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "Game")]
    public class Game : ObjectBase
    {
        protected override void Dispose(bool isDisposing)
        {
            World?.Dispose();
            AssetRegistry?.Dispose();

            Logger.Log(LogLevel.Info, "Game disposed");

            base.Dispose(isDisposing);
        }

        public World? World
        {
            get => this.GetWorld();
            set => this.SetWorld(value);
        }

        public AssetRegistry? AssetRegistry
        {
            get => this.GetAssetRegistry();
            set => this.SetAssetRegistry(value);
        }
    }
}