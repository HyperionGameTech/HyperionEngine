using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class Game
    {
        private Scene? scene;
        private InputManager? inputManager;
        private AssetManager? assetManager;

        protected Scene Scene
        {
            get
            {
                return scene;
            }
        }

        protected InputManager InputManager
        {
            get
            {
                return inputManager;
            }
        }

        protected AssetManager AssetManager
        {
            get
            {
                return assetManager;
            }
        }

        /// <summary>
        /// Invoked from native code before the Init() is called.
        /// Sets up handles used by the Game instance.
        /// </summary>
        /// <param name="sceneHandle">Native handle to the scene</param>
        /// <param name="inputManagerPtr">Native pointer to the input manager</param>
        /// <param name="assetManagerPtr">Native pointer to the asset manager</param>
        internal void BeforeInit(ManagedHandle sceneHandle, IntPtr inputManagerPtr, IntPtr assetManagerPtr)
        {
            scene = new Scene(sceneHandle);
            inputManager = new InputManager(inputManagerPtr);
            assetManager = new AssetManager(assetManagerPtr);
        }

        public abstract void Init();
        public abstract void Update(float deltaTime);
    }
}