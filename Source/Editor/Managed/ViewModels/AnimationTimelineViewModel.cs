using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public readonly record struct AnimationNotifyMarker(int Index, string Name, float Time, float Duration);

    public class AnimationTimelineViewModel : ViewModelBase
    {
        public const string BindPoseChoice = "Bind pose";

        private const int PollIntervalMs = 66;

        private static readonly float[] SpeedChoices = { 0.25f, 0.5f, 1.0f, 2.0f };

        private readonly DispatcherTimer _pollTimer;

        private DelegateHandler? _timelineChangedHandler;

        private bool _isActive;
        private bool _isApplyingState;

        private string _selectedClip = BindPoseChoice;
        private bool _isPlaying;
        private bool _isLooping = true;
        private float _speed = 1.0f;
        private float _time;
        private float _length;
        private float _frameRate = 30.0f;
        private bool _isEditingGraph;

        public AnimationTimelineViewModel()
        {
            TogglePlayCommand = new RelayCommand(() => SetPlaying(!_isPlaying));
            StepBackCommand = new RelayCommand(() => StepFrames(-1));
            StepForwardCommand = new RelayCommand(() => StepFrames(1));

            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            _pollTimer.Tick += (_, _) => RefreshPlayback();
        }

        public event Action? NotifiesChanged;

        public ObservableCollection<string> ClipNames { get; } = new ObservableCollection<string> { BindPoseChoice };

        public IReadOnlyList<float> Speeds => SpeedChoices;

        public List<AnimationNotifyMarker> Notifies { get; private set; } = new List<AnimationNotifyMarker>();

        public ICommand TogglePlayCommand { get; }
        public ICommand StepBackCommand { get; }
        public ICommand StepForwardCommand { get; }

        public string SelectedClip
        {
            get => _selectedClip;
            set
            {
                if (value != null && SetProperty(ref _selectedClip, value) && !_isApplyingState)
                {
                    string clipName = value == BindPoseChoice ? string.Empty : value;

                    PostToState(state => state.SetAnimationByName(new Name(clipName)));
                }
            }
        }

        public bool IsPlaying
        {
            get => _isPlaying;
            private set
            {
                if (SetProperty(ref _isPlaying, value))
                {
                    OnPropertyChanged(nameof(PlayButtonIcon));
                }
            }
        }

        public string PlayButtonIcon => _isPlaying ? "DebugPause" : "Play";

        public bool IsLooping
        {
            get => _isLooping;
            set
            {
                if (SetProperty(ref _isLooping, value) && !_isApplyingState)
                {
                    PostToState(state => state.SetLooping(value));
                }
            }
        }

        public float Speed
        {
            get => _speed;
            set
            {
                if (SetProperty(ref _speed, value) && !_isApplyingState)
                {
                    PostToState(state => state.SetSpeed(value));
                }
            }
        }

        public float Time
        {
            get => _time;
            private set
            {
                if (SetProperty(ref _time, value))
                {
                    OnPropertyChanged(nameof(TimeText));
                }
            }
        }

        public float Length
        {
            get => _length;
            private set
            {
                if (SetProperty(ref _length, value))
                {
                    OnPropertyChanged(nameof(TimeText));
                    OnPropertyChanged(nameof(HasClip));
                    OnPropertyChanged(nameof(CanPlay));
                }
            }
        }

        public float FrameRate => _frameRate;

        public bool HasClip => _length > 0.0f;

        public bool CanPlay => HasClip || _isEditingGraph;

        public bool IsEditingGraph
        {
            get => _isEditingGraph;
            private set
            {
                if (SetProperty(ref _isEditingGraph, value))
                {
                    OnPropertyChanged(nameof(CanPlay));
                }
            }
        }

        public string TimeText => $"{_time:0.00} / {_length:0.00} s  ({(int)MathF.Round(_time * _frameRate)})";

        public void SetActive(bool active)
        {
            if (_isActive == active)
            {
                return;
            }

            _isActive = active;

            _timelineChangedHandler?.Remove();
            _timelineChangedHandler = null;

            if (!active)
            {
                _pollTimer.Stop();

                return;
            }

            EditorAnimationEditState? animationEditState = EngineManager.EditorGame?.EditorSubsystem?.EditorAnimationEditState;

            WeakReference<AnimationTimelineViewModel> weakThis = new WeakReference<AnimationTimelineViewModel>(this);

            _timelineChangedHandler = animationEditState?.GetOnTimelineChangedDelegate().Bind(() =>
            {
                if (weakThis.TryGetTarget(out AnimationTimelineViewModel? target))
                {
                    Dispatcher.UIThread.Post(target.RefreshAll);
                }
            });

            _pollTimer.Start();

            RefreshAll();
        }

        public void Scrub(float time)
        {
            Time = Math.Clamp(time, 0.0f, _length);

            PostToState(state =>
            {
                state.SetPlaying(false);
                state.SetTime(time);
            });
        }

        public void AddNotify(float time) => PostToState(state => state.AddNotify(time));

        public void RemoveNotify(int index) => PostToState(state => state.RemoveNotify((uint)index));

        public void MoveNotify(int index, float time) => PostToState(state => state.SetNotifyTime((uint)index, time));

        public void RenameNotify(int index, string name)
        {
            if (!string.IsNullOrWhiteSpace(name))
            {
                PostToState(state => state.SetNotifyName((uint)index, new Name(name.Trim())));
            }
        }

        private void SetPlaying(bool playing) => PostToState(state => state.SetPlaying(playing));

        private void StepFrames(int numFrames) => PostToState(state => state.StepFrames(numFrames));

        private void PostToState(Action<EditorAnimationEditState> action)
        {
            EditorAnimationEditState? animationEditState = EngineManager.EditorGame?.EditorSubsystem?.EditorAnimationEditState;

            if (animationEditState == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (animationEditState.IsActive())
                {
                    action(animationEditState);
                }
            }).ContinueWith(_ => Dispatcher.UIThread.Post(RefreshPlayback));
        }

        private void RefreshPlayback()
        {
            EditorAnimationEditState? animationEditState = EngineManager.EditorGame?.EditorSubsystem?.EditorAnimationEditState;

            if (!_isActive || animationEditState == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!animationEditState.IsActive())
                {
                    return;
                }

                bool isPlaying = animationEditState.IsPlaying();
                float time = animationEditState.GetTime();
                float length = animationEditState.GetLength();

                Dispatcher.UIThread.Post(() =>
                {
                    IsPlaying = isPlaying;
                    Length = length;
                    Time = time;
                });
            });
        }

        private void RefreshAll()
        {
            EditorAnimationEditState? animationEditState = EngineManager.EditorGame?.EditorSubsystem?.EditorAnimationEditState;

            if (!_isActive || animationEditState == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!animationEditState.IsActive())
                {
                    return;
                }

                List<string> clipNames = new List<string> { BindPoseChoice };

                foreach (Name name in animationEditState.GetClipNames())
                {
                    clipNames.Add(name.ToString());
                }

                Animation? animation = animationEditState.GetAnimation();
                string selectedClip = animation != null && animation.IsValid ? animation.Name.ToString() : BindPoseChoice;

                List<AnimationNotifyMarker> notifies = new List<AnimationNotifyMarker>();

                for (uint index = 0; index < animationEditState.NumNotifies(); ++index)
                {
                    notifies.Add(new AnimationNotifyMarker(
                        (int)index,
                        animationEditState.GetNotifyName(index).ToString(),
                        animationEditState.GetNotifyTime(index),
                        animationEditState.GetNotifyDuration(index)));
                }

                bool isLooping = animationEditState.IsLooping();
                float speed = animationEditState.GetSpeed();
                float frameRate = animationEditState.GetFrameRate();
                bool isEditingGraph = animationEditState.IsEditingGraph();

                Dispatcher.UIThread.Post(() =>
                {
                    _isApplyingState = true;

                    if (!System.Linq.Enumerable.SequenceEqual(ClipNames, clipNames))
                    {
                        ClipNames.Clear();

                        foreach (string clipName in clipNames)
                        {
                            ClipNames.Add(clipName);
                        }
                    }

                    SelectedClip = selectedClip;
                    IsLooping = isLooping;
                    Speed = speed;

                    _isApplyingState = false;

                    _frameRate = frameRate;
                    IsEditingGraph = isEditingGraph;

                    Notifies = notifies;
                    NotifiesChanged?.Invoke();

                    RefreshPlayback();
                });
            });
        }
    }
}
