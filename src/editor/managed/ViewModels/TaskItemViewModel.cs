using System;

namespace Hyperion.Editor.ViewModels
{
    public class TaskItemViewModel : ViewModelBase
    {
        private ObjIdBase _taskId;
        private string _taskName;
        private float _progress;

        public TaskItemViewModel(ObjIdBase taskId, string taskName)
        {
            _taskId = taskId;
            _taskName = taskName;
            _progress = 0.0f;
        }

        public ObjIdBase TaskId => _taskId;

        public string TaskName => _taskName;

        public float Progress
        {
            get => _progress;
            set => SetProperty(ref _progress, value);
        }
    }
}
