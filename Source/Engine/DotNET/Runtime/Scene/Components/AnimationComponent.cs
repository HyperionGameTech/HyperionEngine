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
    [StructLayout(LayoutKind.Sequential, Size = 72)]
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
        public float TwistAngle = 0.0f;
        public Name TwistRootBone;
        public Name TwistEndBone;

        public AnimationPlaybackState()
        {
        }
    }

    [ClassBinding(Name="AnimationComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 96)]
    public ref struct AnimationComponent : IComponent
    {
        public AnimationPlaybackState PlaybackState = new();

        private Handle<Animation> _animationHandle;

        private Handle<BoneMask> _layerMaskHandle;

        private float _crossFadeRemaining;

        public AnimationComponent()
        {
        }

        public void Dispose()
        {
            _animationHandle.Dispose();
            _layerMaskHandle.Dispose();
        }

        public Animation? Animation
        {
            get => _animationHandle.GetValue();
            set
            {
                _animationHandle.Dispose();

                if (value == null)
                {
                    _animationHandle = Handle<Animation>.Empty;

                    return;
                }

                _animationHandle = new Handle<Animation>(value);
            }
        }

        public BoneMask? LayerMask
        {
            get => _layerMaskHandle.GetValue();
            set
            {
                _layerMaskHandle.Dispose();
                _layerMaskHandle = value != null ? new Handle<BoneMask>(value) : Handle<BoneMask>.Empty;
            }
        }

        public bool IsPlaying => PlaybackState.Status == AnimationPlaybackStatus.Playing;

        public void Play(Animation? clip, AnimationLoopMode loopMode = AnimationLoopMode.Repeat, float speed = 1.0f)
        {
            Animation = clip;

            PlaybackState.Status = clip != null ? AnimationPlaybackStatus.Playing : AnimationPlaybackStatus.Stopped;
            PlaybackState.LoopMode = loopMode;
            PlaybackState.Speed = speed;
            PlaybackState.CurrentTime = 0.0f;

            _crossFadeRemaining = 0.0f;
        }

        public void CrossFade(Animation? clip, float duration, AnimationLoopMode loopMode = AnimationLoopMode.Repeat, float speed = 1.0f)
        {
            bool wasPlaying = IsPlaying;

            Play(clip, loopMode, speed);

            _crossFadeRemaining = wasPlaying && clip != null ? duration : 0.0f;
        }

        public void Stop()
        {
            PlaybackState.Status = AnimationPlaybackStatus.Stopped;
        }
    }
}
