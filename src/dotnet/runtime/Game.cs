using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "Game")]
    public abstract class Game : ObjectBase
    {
        public World World
        {
            get => this.GetWorld();
        }

        public abstract void OnLaunch();
        public abstract void OnUpdate(float deltaTime);
    }
}