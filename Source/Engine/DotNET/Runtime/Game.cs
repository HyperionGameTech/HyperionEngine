using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "Game")]
    public class Game : ObjectBase
    {
        protected override void Dispose(bool isDisposing)
        {
            if (IsValid)
            {
                World?.Dispose();
                AssetRegistry?.Dispose();
            }

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