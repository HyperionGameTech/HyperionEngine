using System;
using System.Windows.Input;
using System.Threading.Tasks;
using Avalonia.Threading;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ForegroundTaskViewModel : ViewModelBase
    {
        private ObjIdBase _taskId;
        private string _taskName;
        private string _title;
        private string _description;
        private float _progress;
        private float _opacity;
        private bool _isVisible;
        private EditorTaskBase? _task;
        private DelegateHandler? _onDescriptionChangedHandler;

        public ForegroundTaskViewModel()
        {
            _taskId = default;
            _taskName = string.Empty;
            _title = string.Empty;
            _description = string.Empty;
            _progress = 0.0f;
            _opacity = 0.0f;
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

        public bool IsVisible
        {
            get => _isVisible;
            set => SetProperty(ref _isVisible, value);
        }

        public float Opacity
        {
            get => _opacity;
            set => SetProperty(ref _opacity, value);
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
            Dispatcher.UIThread.VerifyAccess();
            
            _onDescriptionChangedHandler?.Remove();
            
            _task = task;
            TaskId = task.Id;
            TaskName = task.Class.Name.ToString();
            Title = task.Title.ToString();
            Description = task.Description.ToString();
            Progress = task.Progress;
            Opacity = 1.0f;
            IsVisible = true;

            WeakReference<EditorTaskBase> weakTask = new WeakReference<EditorTaskBase>(task);

            _onDescriptionChangedHandler = task.GetOnDescriptionChangeDelegate().Bind(() =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    // check if the current task is still the same (it could have been replaced by another task)
                    if (!weakTask.TryGetTarget(out EditorTaskBase? currentTask) || currentTask.Id != TaskId)
                    {
                        return;
                    }

                    Description = currentTask.Description.ToString();
                });
            });
        }

        public void Clear()
        {
            Dispatcher.UIThread.VerifyAccess();

            Opacity = 0.0f;
            
            _onDescriptionChangedHandler?.Remove();
            _onDescriptionChangedHandler = null;

            // hide upon animation completion
            // we check if the task is still the same before hiding, as it could have been replaced by another task in the meantime
            WeakReference<EditorTaskBase?> weakTask = new WeakReference<EditorTaskBase?>(_task);
            Dispatcher.UIThread.Post(async () =>
            {
                await Task.Delay(300);

                if (weakTask.TryGetTarget(out EditorTaskBase? currentTask) && currentTask != null && currentTask.Id == TaskId)
                {
                    return;
                }
                
                IsVisible = false;
                _task = null;
                TaskId = default;
                TaskName = string.Empty;
                Title = string.Empty;
                Description = string.Empty;
                Progress = 0.0f;
            });
        }
    }
}
