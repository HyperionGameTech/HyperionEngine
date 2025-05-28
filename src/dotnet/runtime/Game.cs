using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class Game
    {
        private Scene? scene;
        private InputManager? inputManager;
        private AssetManager? assetManager;
        private UIStage? uiStage;

        protected Scene Scene
        {
            get
            {
                return scene!;
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
        internal void BeforeInit(Scene scene, InputManager inputManager, AssetManager assetManager, UIStage uiStage)
        {
            this.scene = scene;
            this.inputManager = inputManager;
            this.assetManager = assetManager;
            this.uiStage = uiStage;
        }

        public abstract void Init();
        public abstract void Update(float deltaTime);
    }
}