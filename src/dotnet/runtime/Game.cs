using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class Game
    {
        public abstract void Init();
        public abstract void Update(float deltaTime);
    }
}