using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Game")]
    public abstract class Game : HypObject
    {
        private World? world;
        private InputManager? inputManager;
        private AssetManager? assetManager;
        private UIStage? uiStage;

        protected World World
        {
            get
            {
                return world!;
            }
        }

        protected InputManager InputManager
        {
            get
            {
                return inputManager!;
            }
        }

        protected AssetManager AssetManager
        {
            get
            {
                return assetManager!;
            }
        }

        protected UIStage UIStage
        {
            get
            {
                return uiStage;
            }
        }

        /// <summary>
        /// Invoked from native code before the Init() is called.
        /// Sets up handles used by the Game instance.
        internal void BeforeInit(World world, InputManager inputManager, AssetManager assetManager, UIStage uiStage)
        {
            this.world = world;
            this.inputManager = inputManager;
            this.assetManager = assetManager;
            this.uiStage = uiStage;
        }

        public abstract void Init();
        public abstract void Update(float deltaTime);
    }
}