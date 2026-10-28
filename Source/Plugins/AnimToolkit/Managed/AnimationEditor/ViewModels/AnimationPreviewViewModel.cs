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

        private readonly Entity _entity;
        private readonly DispatcherTimer _pollTimer;

        private bool _isPlaying;
        private float _time;
        private float _length;
        private bool _isApplyingTime;
        private bool _canOpenGraph;
        private bool _isShown;

        public AnimationPreviewViewModel(Entity entity)
            : base("Preview", default, null, null, false)
        {
            _entity = entity;

            TogglePlayCommand = new RelayCommand(TogglePlay);
            EditCommand = new RelayCommand(() => AnimationEditorWiring.RequestEditAnimation(state => state.EnterEntity(_entity)));
            OpenGraphCommand = new RelayCommand(() => AnimationEditorWiring.OpenEntityGraph(_entity));

            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            _pollTimer.Tick += (_, _) => RefreshValue();
        }

        public override bool SupportsMultipleTargets => false;

        public ICommand TogglePlayCommand { get; }

        public ICommand EditCommand { get; }

        public ICommand OpenGraphCommand { get; }

        public bool CanOpenGraph
        {
            get => _canOpenGraph;
            private set => SetProperty(ref _canOpenGraph, value);
        }

        public bool IsPlaying
        {
            get => _isPlaying;
            private set
            {
                if (SetProperty(ref _isPlaying, value))
                {
                    OnPropertyChanged(nameof(PlayButtonText));
                }
            }
        }

        public string PlayButtonText => _isPlaying ? "Pause" : "Play";

        public float Length
        {
            get => _length;
            private set => SetProperty(ref _length, value);
        }

        public float Time
        {
            get => _time;
            set
            {
                if (SetProperty(ref _time, value))
                {
                    OnPropertyChanged(nameof(TimeText));

                    if (!_isApplyingTime)
                    {
                        Scrub(value);
                    }
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
                _pollTimer.Stop();
            }
        }

        public override void RefreshValue()
        {
            AnimationEditorExtension? animationExtension = EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditorExtension();

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

                Dispatcher.UIThread.Post(() =>
                {
                    CanOpenGraph = hasGraph;

                    ApplyState(isActive, isPlaying, time, length);
                });
            });
        }

        private void ApplyState(bool isActive, bool isPlaying, float time, float length)
        {
            IsPlaying = isPlaying;
            Length = length;

            _isApplyingTime = true;
            Time = time;
            _isApplyingTime = false;

            OnPropertyChanged(nameof(TimeText));

            if (_isShown && isActive && isPlaying)
            {
                _pollTimer.Start();
            }
            else
            {
                _pollTimer.Stop();
            }
        }

        private void TogglePlay()
        {
            AnimationEditorExtension? animationExtension = EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditorExtension();

            if (animationExtension == null)
            {
                return;
            }

            bool play = !_isPlaying;

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
            AnimationEditorExtension? animationExtension = EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditorExtension();

            if (animationExtension == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!animationExtension.IsAnimationPreviewActive(_entity))
                {
                    animationExtension.StartAnimationPreview(_entity);
                }

                animationExtension.SetAnimationPreviewPlaying(false);
                animationExtension.SetAnimationPreviewTime(time);
            }).ContinueWith(_ => Dispatcher.UIThread.Post(RefreshValue));
        }
    }
}
