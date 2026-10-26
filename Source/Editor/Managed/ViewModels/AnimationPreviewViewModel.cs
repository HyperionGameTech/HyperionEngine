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

        public AnimationPreviewViewModel(Entity entity)
            : base("Preview", default, null, null, false)
        {
            _entity = entity;

            TogglePlayCommand = new RelayCommand(TogglePlay);

            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            _pollTimer.Tick += (_, _) => RefreshValue();
        }

        public override bool SupportsMultipleTargets => false;

        public ICommand TogglePlayCommand { get; }

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

        public override void RefreshValue()
        {
            EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

            if (editorSubsystem == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                bool isActive = editorSubsystem.IsAnimationPreviewActive(_entity);
                bool isPlaying = isActive && editorSubsystem.IsAnimationPreviewPlaying();
                float time = isActive ? editorSubsystem.GetAnimationPreviewTime() : 0.0f;
                float length = editorSubsystem.GetAnimationClipLength(_entity);

                Dispatcher.UIThread.Post(() => ApplyState(isActive, isPlaying, time, length));
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

            if (isActive && isPlaying)
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
            EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

            if (editorSubsystem == null)
            {
                return;
            }

            bool play = !_isPlaying;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!editorSubsystem.IsAnimationPreviewActive(_entity))
                {
                    editorSubsystem.StartAnimationPreview(_entity);
                }

                editorSubsystem.SetAnimationPreviewPlaying(play);
            }).ContinueWith(_ => Dispatcher.UIThread.Post(RefreshValue));
        }

        private void Scrub(float time)
        {
            EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

            if (editorSubsystem == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!editorSubsystem.IsAnimationPreviewActive(_entity))
                {
                    editorSubsystem.StartAnimationPreview(_entity);
                }

                editorSubsystem.SetAnimationPreviewPlaying(false);
                editorSubsystem.SetAnimationPreviewTime(time);
            }).ContinueWith(_ => Dispatcher.UIThread.Post(RefreshValue));
        }
    }
}
