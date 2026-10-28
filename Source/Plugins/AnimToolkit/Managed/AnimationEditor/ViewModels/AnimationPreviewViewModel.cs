using System;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class AnimationPreviewViewModel : InspectorPropertyViewModelBase
    {
        private const int PollIntervalMs = 100;
        private const int ScrubHoldMs = 300;

        private readonly Entity _entity;

        private DispatcherTimer? _pollTimer;
        private bool _isShown;
        private bool _isPlaying;
        private bool _hasGraph;
        private float _time;
        private float _length;
        private bool _isApplyingTime;
        private DateTime _lastScrub = DateTime.MinValue;

        public AnimationPreviewViewModel(Entity entity)
            : base("Preview", default, null, null, false)
        {
            _entity = entity;

            TogglePlayCommand = new RelayCommand(TogglePlay);
            EditCommand = new RelayCommand(() => AnimationEditorWiring.RequestEditAnimation(state => state.EnterEntity(_entity)));
            OpenGraphCommand = new RelayCommand(() => AnimationEditorWiring.OpenEntityGraph(_entity));
        }

        public override bool SupportsMultipleTargets => false;

        public override bool ShowInlineLabel => false;

        public ICommand TogglePlayCommand { get; }

        public ICommand EditCommand { get; }

        public ICommand OpenGraphCommand { get; }

        public bool HasGraph
        {
            get => _hasGraph;
            private set
            {
                if (SetProperty(ref _hasGraph, value))
                {
                    OnPropertyChanged(nameof(HasClip));
                }
            }
        }

        public bool HasClip => !_hasGraph && _length > 0.0f;

        public bool IsPlaying
        {
            get => _isPlaying;
            private set
            {
                if (SetProperty(ref _isPlaying, value))
                {
                    OnPropertyChanged(nameof(PlayIcon));
                }
            }
        }

        public string PlayIcon => _isPlaying ? "DebugPause" : "Play";

        public float Length
        {
            get => _length;
            private set
            {
                if (SetProperty(ref _length, value))
                {
                    OnPropertyChanged(nameof(HasClip));
                    OnPropertyChanged(nameof(TimeText));
                }
            }
        }

        public float Time
        {
            get => _time;
            set
            {
                if (!SetProperty(ref _time, value))
                {
                    return;
                }

                OnPropertyChanged(nameof(TimeText));

                if (!_isApplyingTime)
                {
                    _lastScrub = DateTime.UtcNow;
                    Scrub(value);
                }
            }
        }

        public string TimeText => $"{_time:0.00} / {_length:0.00}";

        public void SetShown(bool shown)
        {
            _isShown = shown;

            if (shown)
            {
                RefreshValue();
            }
            else
            {
                _pollTimer?.Stop();
            }
        }

        private static AnimationEditorExtension? GetExtension() => EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditorExtension();

        public override void RefreshValue()
        {
            AnimationEditorExtension? animationExtension = GetExtension();

            if (animationExtension == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                bool isActive = animationExtension.IsAnimationPreviewActive(_entity);
                bool isPlaying = isActive && animationExtension.IsAnimationPreviewPlaying();
                float time = isActive ? animationExtension.GetAnimationPreviewTime() : 0.0f;
                float length = animationExtension.GetAnimationClipLength(_entity);
                bool hasGraph = _entity.TryGetComponent(out AnimationComponent animationComponent) && animationComponent.Graph != null;

                Dispatcher.UIThread.Post(() => ApplyState(isPlaying, time, length, hasGraph));
            });
        }

        private void ApplyState(bool isPlaying, float time, float length, bool hasGraph)
        {
            HasGraph = hasGraph;
            IsPlaying = isPlaying;
            Length = length;

            if ((DateTime.UtcNow - _lastScrub).TotalMilliseconds > ScrubHoldMs)
            {
                _isApplyingTime = true;
                Time = time;
                _isApplyingTime = false;
            }

            if (isPlaying)
            {
                _pollTimer ??= CreatePollTimer();
                _pollTimer.Start();
            }
            else
            {
                _pollTimer?.Stop();
            }
        }

        private DispatcherTimer CreatePollTimer()
        {
            DispatcherTimer timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            timer.Tick += (_, _) => RefreshValue();

            return timer;
        }

        private void TogglePlay()
        {
            AnimationEditorExtension? animationExtension = GetExtension();

            if (animationExtension == null)
            {
                return;
            }

            bool play = !_isPlaying;

            IsPlaying = play;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!animationExtension.IsAnimationPreviewActive(_entity))
                {
                    animationExtension.StartAnimationPreview(_entity);
                }

                animationExtension.SetAnimationPreviewPlaying(play);
            }).ContinueWith(_ => Dispatcher.UIThread.Post(RefreshValue));
        }

        private void Scrub(float time)
        {
            AnimationEditorExtension? animationExtension = GetExtension();

            if (animationExtension == null)
            {
                return;
            }

            IsPlaying = false;
            _pollTimer?.Stop();

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!animationExtension.IsAnimationPreviewActive(_entity))
                {
                    animationExtension.StartAnimationPreview(_entity);
                }

                animationExtension.SetAnimationPreviewPlaying(false);
                animationExtension.SetAnimationPreviewTime(time);
            });
        }
    }
}
