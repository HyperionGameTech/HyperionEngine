using System;
using System.Windows.Input;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ForegroundTaskViewModel : ViewModelBase
    {
        private ObjIdBase _taskId;
        private string _taskName;
        private float _progress;
        private bool _isVisible;
        private EditorTaskBase? _task;

        public ForegroundTaskViewModel()
        {
            _taskId = default;
            _taskName = string.Empty;
            _progress = 0.0f;
            _isVisible = false;
            CancelCommand = new RelayCommand(OnCancel);
        }

        public ObjIdBase TaskId
        {
            get => _taskId;
            set => SetProperty(ref _taskId, value);
        }

        public string TaskName
        {
            get => _taskName;
            set => SetProperty(ref _taskName, value);
        }

        public float Progress
        {
            get => _progress;
            set => SetProperty(ref _progress, value);
        }

        public bool IsVisible
        {
            get => _isVisible;
            set => SetProperty(ref _isVisible, value);
        }

        public ICommand CancelCommand { get; }

        private void OnCancel()
        {
            if (_task != null)
            {
                _task.Cancel();
            }
        }

        public void SetTask(EditorTaskBase task)
        {
            _task = task;
            TaskId = task.Id;
            TaskName = task.Class.Name.ToString();
            Progress = task.Progress;
            IsVisible = true;
        }

        public void Clear()
        {
            IsVisible = false;
            _task = null;
            TaskId = default;
            TaskName = string.Empty;
            Progress = 0.0f;
        }
    }
}
