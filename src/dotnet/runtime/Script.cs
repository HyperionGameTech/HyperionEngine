using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class Script
    {
        private Scene? scene;
        private IDBase entity;

        protected Scene Scene
        {
            get
            {
                return scene!;
            }
        }

        protected IDBase Entity
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
        /// <param name="scene">Native handle to the scene</param>
        internal void BeforeInit(Scene scene)
        {
            this.scene = scene;

            Logger.Log(LogType.Info, "{0} BeforeInit: this.scene = {1}", GetType().Name, this.scene);
        }

        public virtual void Init(IDBase entity)
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