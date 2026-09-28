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

    [ClassBinding(Name="AnimationLoopMode")]
    public enum AnimationLoopMode : byte
    {
        Once = 0,
        Repeat
    }

    [ClassBinding(Name="AnimationPlaybackState")]
    [StructLayout(LayoutKind.Sequential, Size = 104)]
    public struct AnimationPlaybackState
    {
        public uint AnimationIndex = uint.MaxValue;
        public AnimationPlaybackStatus Status = AnimationPlaybackStatus.Stopped;
        public AnimationLoopMode LoopMode = AnimationLoopMode.Once;
        public float Speed = 1.0f;
        public float CurrentTime = 0.0f;
        public uint LayerAnimationIndex = uint.MaxValue;
        public float LayerTime = 0.0f;
        public float LayerWeight = 0.0f;
        public Name LayerExcludedBone;
        public uint SecondLayerAnimationIndex = uint.MaxValue;
        public float SecondLayerTime = 0.0f;
        public float SecondLayerWeight = 0.0f;
        public uint OverlayAnimationIndex = uint.MaxValue;
        public float OverlayTime = 0.0f;
        public float OverlayWeight = 0.0f;
        public Name OverlayRootBone;
        public Name OverlaySecondRootBone;
        public float TwistAngle = 0.0f;
        public Name TwistRootBone;
        public Name TwistEndBone;

        public AnimationPlaybackState()
        {
        }
    }

    [ClassBinding(Name="AnimationComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 104)]
    public ref struct AnimationComponent : IComponent
    {
        public AnimationPlaybackState PlaybackState = new();

        public AnimationComponent()
        {
        }
    }
}
