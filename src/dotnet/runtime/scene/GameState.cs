using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum GameStateMode : uint
    {
        Stopped = 0,
        Simulating = 1,
        Paused = 2,
        EditMode = 3
    }

    [ClassBinding(Name="GameState")]
    [StructLayout(LayoutKind.Sequential)]
    public struct GameState
    {
        private GameStateMode _mode;
        private float _deltaTime;
        private float _gameTime;

        public GameState()
        {
        }

        public GameStateMode Mode => _mode;

        public bool Stopped => _mode == GameStateMode.Stopped;
        public bool Simulating => _mode == GameStateMode.Simulating;
        public bool Paused => _mode == GameStateMode.Paused;
        public bool EditMode => _mode == GameStateMode.EditMode;

        public float DeltaTime => _deltaTime;

        public float GameTime => _gameTime;
    }
}