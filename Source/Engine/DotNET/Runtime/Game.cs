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

        public UISubsystem? UISubsystem => this.GetUISubsystem();

        /// <summary>
        /// Called on the sim thread once the World is ready, before the engine sets up a View for the
        /// primary camera
        /// </summary>
        [ScriptMethodStub]
        protected virtual void OnLaunch()
        {
        }

        /// <summary>
        /// Called on the sim thread every tick after the game has launched
        /// </summary>
        [ScriptMethodStub]
        protected virtual void OnUpdate(float delta)
        {
        }

        /// <summary>
        /// Called on the sim thread when the game shuts down, before the World is torn down
        /// </summary>
        [ScriptMethodStub]
        protected virtual void OnShutdown()
        {
        }
    }
}
