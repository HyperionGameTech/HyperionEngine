using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "Game")]
    public abstract class Game : ObjectBase
    {
        public World World
        {
            get
            {
                return this.GetWorld();
            }
        }

        public abstract void OnLaunch();
        public abstract void OnUpdate(float deltaTime);
    }
}