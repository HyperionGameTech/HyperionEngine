using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum GameStateMode : uint
    {
        Editor = 0,
        Simulating = 1
    }

    [HypClassBinding(Name="GameState")]
    [StructLayout(LayoutKind.Sequential)]
    public struct GameState
    {
        public GameStateMode mode;
        public float deltaTime;
        public float gameTime;

        public GameState()
        {
        }

        public GameStateMode Mode
        {
            get { return mode; }
        }

        public float DeltaTime
        {
            get { return deltaTime; }
        }

        public float GameTime
        {
            get { return gameTime; }
        }
    }
}