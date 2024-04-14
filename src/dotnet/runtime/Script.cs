using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class Script
    {
        private Scene? scene;
        private Entity entity;

        protected Scene Scene
        {
            get
            {
                return scene!;
            }
        }

        protected Entity Entity
        {
            get
            {
                return entity;
            }
        }

        /// <summary>
        /// Invoked from native code before the Init() is called.
        /// Sets up handles used by the Script instance.
        /// </summary>
        /// <param name="sceneHandle">Native handle to the scene</param>
        internal void BeforeInit(ManagedHandle sceneHandle)
        {
            scene = new Scene(sceneHandle);
        }

        public virtual void Init(Entity entity)
        {
            this.entity = entity;
        }

        [Hyperion.ScriptMethodStub]
        public virtual void Destroy()
        {
            // Do nothing
        }

        [Hyperion.ScriptMethodStub]
        public virtual void Update(float deltaTime)
        {
            // Do nothing
        }
    }
}