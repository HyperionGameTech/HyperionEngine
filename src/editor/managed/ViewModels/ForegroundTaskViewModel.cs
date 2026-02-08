using System;

namespace Hyperion.Editor.ViewModels
{
    public class ForegroundTaskViewModel : ViewModelBase
    {
        private ObjIdBase _taskId;
        private string _taskName;
        private float _progress;
        private bool _isVisible;

        public ForegroundTaskViewModel()
        {
            _taskId = default;
            _taskName = string.Empty;
            _progress = 0.0f;
            _isVisible = false;
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

        public void SetTask(ObjIdBase taskId, string taskName)
        {
            TaskId = taskId;
            TaskName = taskName;
            Progress = 0.0f;
            IsVisible = true;
        }

        public void Clear()
        {
            IsVisible = false;
            TaskId = default;
            TaskName = string.Empty;
            Progress = 0.0f;
        }
    }
}
