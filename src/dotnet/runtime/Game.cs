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
        internal void BeforeInit(Scene scene, InputManager inputManager, AssetManager assetManager, RefCountedPtr uiStageRc)
        {
            this.scene = scene;
            this.inputManager = inputManager;
            this.assetManager = assetManager;
            this.uiStage = new UIStage(uiStageRc);

            Console.WriteLine("this.inputManager.HypClass = {0} {1}", this.inputManager.HypClass, this.inputManager.HypClass.Name);
            Console.WriteLine("this.assetManager.HypClass = {0} {1}", this.assetManager.HypClass, this.assetManager.HypClass.Name);

            Console.WriteLine("Game BeforeInit: this.scene = {0}, this.inputManager = {1}", this.scene, this.inputManager);
        }

        public abstract void Init();
        public abstract void Update(float deltaTime);
    }
}