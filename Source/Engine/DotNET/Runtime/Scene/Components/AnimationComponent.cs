using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="AnimationPlaybackStatus")]
    public enum AnimationPlaybackStatus : byte
    {
        Stopped = 0,
        Paused,
        Playing
    }

    [ClassBinding(Name="AnimationPlaybackStatus")]
    public enum AnimationLoopMode : byte
    {
        Once = 0,
        Repeat
    }
    
    [ClassBinding(Name="AnimationPlaybackState")]
    public struct AnimationPlaybackState
    {
        public uint AnimationIndex;
        public AnimationPlaybackStatus Status;
        public AnimationLoopMode LoopMode;
        public float Speed;
        public float CurrentTime;
    }

    [ClassBinding(Name="AnimationComponent")]
    public ref struct AnimationComponent : IComponent
    {
        public AnimationPlaybackState PaybackState;

        public AnimationComponent()
        {
        }

        public void Dispose()
        {
        }

        public static Class Class => Class.GetClass(typeof(AnimationComponent));

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (AnimationComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }
    }
}
