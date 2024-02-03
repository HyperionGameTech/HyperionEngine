using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class Game
    {
        public abstract void Initialize();
        public abstract void Update(float deltaTime);
    }
}