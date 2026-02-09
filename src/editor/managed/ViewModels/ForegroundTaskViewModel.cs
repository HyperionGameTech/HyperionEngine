using System;
using System.Collections.Concurrent;
using System.Windows.Input;
using System.Threading.Tasks;
using Avalonia.Threading;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ForegroundTaskState : ViewModelBase
    {
        private string _title;
        private string _description;
        private float _progress;
        private float _opacity;

        public ObjIdBase TaskId { get; }
        public EditorTaskBase? Task { get; }
        public DelegateHandler? DescriptionChangedHandler { get; set; }

        public ForegroundTaskState()
        {
            TaskId = ObjIdBase.Invalid;
            Task = null;
            _title = string.Empty;
            _description = string.Empty;
            _progress = 0.0f;
            _opacity = 0.0f;
        }

        public ForegroundTaskState(EditorTaskBase task)
        {
            TaskId = task.Id;
            Task = task;
            _title = task.Title.ToString();
            _description = task.Description.ToString();
            _progress = task.Progress;
            _opacity = 1.0f;
        }

        public string Title
        {
            get => _title;
            set => SetProperty(ref _title, value);
        }

        public string Description
        {
            get => _description;
            set => SetProperty(ref _description, value);
        }

        public float Progress
        {
            get => _progress;
            set => SetProperty(ref _progress, value);
        }

        public float Opacity
        {
            get => _opacity;
            set => SetProperty(ref _opacity, value);
        }
    }

    public class ForegroundTaskViewModel : ViewModelBase
    {
        private static readonly ForegroundTaskState _defaultTaskState = new ForegroundTaskState();

        private readonly ConcurrentDictionary<ObjIdBase, ForegroundTaskState> _tasks;
        private ForegroundTaskState _currentTask;
        private bool _isVisible;

        public ForegroundTaskViewModel()
        {
            _tasks = new ConcurrentDictionary<ObjIdBase, ForegroundTaskState>();
            _currentTask = _defaultTaskState;
            _isVisible = false;
            CancelCommand = new RelayCommand(OnCancel);
        }

        public ForegroundTaskState CurrentTask
        {
            get => _currentTask;
            private set => SetProperty(ref _currentTask, value);
        }

        public bool IsVisible
        {
            get => _isVisible;
            private set => SetProperty(ref _isVisible, value);
        }

        public ICommand CancelCommand { get; }

        private void OnCancel()
        {
            _currentTask.Task?.Cancel();
        }

        public void SetTask(EditorTaskBase task)
        {
            Dispatcher.UIThread.VerifyAccess();

            var taskState = new ForegroundTaskState(task);
            
            if (_tasks.TryAdd(task.Id, taskState))
            {
                taskState.DescriptionChangedHandler = task.GetOnDescriptionChangeDelegate().Bind(() =>
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        if (_tasks.TryGetValue(task.Id, out var state))
                        {
                            state.Description = task.Description.ToString();
                        }
                    });
                });

                CurrentTask = taskState;
                IsVisible = true;
            }
        }

        public void UpdateProgress(ObjIdBase taskId, float progress)
        {
            if (_tasks.TryGetValue(taskId, out var taskState))
            {
                taskState.Progress = progress;
            }
        }

        public void Remove(ObjIdBase taskId)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (_tasks.TryGetValue(taskId, out var taskState))
            {
                taskState.Opacity = 0.0f;

                taskState.DescriptionChangedHandler?.Remove();
                taskState.DescriptionChangedHandler = null;

                Dispatcher.UIThread.Post(async () =>
                {
                    await Task.Delay(300);

                    _tasks.TryRemove(taskId, out _);

                    if (_currentTask.TaskId.Equals(taskId) || !_currentTask.TaskId.IsValid)
                    {
                        if (!_tasks.IsEmpty)
                        {
                            foreach (var kvp in _tasks)
                            {
                                if (kvp.Value.Task != null && !kvp.Value.Task.IsCompleted())
                                {
                                    CurrentTask = kvp.Value;
                                    IsVisible = true;
                                    return;
                                }
                            }
                        }

                        CurrentTask = _defaultTaskState;
                        IsVisible = false;
                    }
                });
            }
        }
    }
}
