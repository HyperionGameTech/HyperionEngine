using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="AudioPlaybackStatus")]
    public enum AudioPlaybackStatus : byte
    {
        Stopped = 0,
        Paused = 1,
        Playing = 2
    }

    [ClassBinding(Name="AudioLoopMode")]
    public enum AudioLoopMode : byte
    {
        Once = 0,
        Repeat = 1
    }

    [ClassBinding(Name="AudioPlaybackState")]
    [StructLayout(LayoutKind.Sequential)]
    public ref struct AudioPlaybackState
    {
        public AudioPlaybackStatus Status;
        public AudioLoopMode LoopMode;
        public float Speed;
        public float CurrentTime;
    }

    [ClassBinding(Name="AudioComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 64, Pack = 16)]
    public ref struct AudioComponent : IComponent
    {
        [FieldOffset(0)]
        public Handle<AudioSource> AudioSource;

        [FieldOffset(8)]
        public AudioPlaybackState PlaybackState;

        [FieldOffset(32)]
        public Vec3f LastPosition;

        [FieldOffset(48)]
        public float Timer;

        public void Dispose()
        {
            AudioSource.Dispose();
        }
    }
}