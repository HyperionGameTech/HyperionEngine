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
    [StructLayout(LayoutKind.Sequential)]
    public ref struct AudioComponent : IComponent
    {
        public static Class Class => Class.GetClass(typeof(AudioComponent));

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (AudioComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }

        public Handle<AudioSource> AudioSource;
        public AudioPlaybackState PlaybackState;
        public Vec3f LastPosition;
        public float Timer;

        public void Dispose()
        {
        }
    }
}