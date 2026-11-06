using System;
using Avalonia.Threading;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class TaskItemViewModel : ViewModelBase
    {
        private readonly ObjIdBase _taskId;
        private readonly EditorTaskBase _task;
        private readonly string _title;
        private string _description;
        private float _progress;
        private bool _isCancelling;
        private DelegateHandler? _descriptionChangedHandler;

        public TaskItemViewModel(EditorTaskBase task)
        {
            _task = task;
            _taskId = task.Id;
            _progress = task.Progress;

            string title = task.Title.ToString();
            if (string.IsNullOrWhiteSpace(title))
            {
                title = task.Class.Name.ToString();
            }

            _title = title;
            _description = task.Description.ToString();

            _descriptionChangedHandler = task.GetOnDescriptionChangeDelegate().Bind(() =>
            {
                string description = task.Description.ToString();
                Dispatcher.UIThread.Post(() => Description = description);
            });

            CancelCommand = new RelayCommand(OnCancel, () => !_isCancelling);
        }

        public ObjIdBase TaskId => _taskId;

        public string Title => _title;

        public string Description
        {
            get => _description;
            private set
            {
                if (SetProperty(ref _description, value))
                {
                    OnPropertyChanged(nameof(HasDescription));
                    OnPropertyChanged(nameof(ToolTipText));
                }
            }
        }

        public bool HasDescription => !string.IsNullOrWhiteSpace(_description);

        public string ToolTipText
        {
            get
            {
                if (!HasDescription)
                {
                    return _title;
                }

                return $"{_title}\n{_description}";
            }
        }

        public float Progress
        {
            get => _progress;
            set
            {
                if (SetProperty(ref _progress, value))
                {
                    OnPropertyChanged(nameof(IsIndeterminate));
                    OnPropertyChanged(nameof(ProgressText));
                }
            }
        }

        public bool IsIndeterminate => _progress <= 0.0f;

        public string ProgressText
        {
            get
            {
                if (IsIndeterminate)
                {
                    return string.Empty;
                }

                return $"{Math.Clamp(_progress, 0.0f, 1.0f):P0}";
            }
        }

        public bool IsCancelling
        {
            get => _isCancelling;
            private set => SetProperty(ref _isCancelling, value);
        }

        public RelayCommand CancelCommand { get; }

        private void OnCancel()
        {
            if (_isCancelling)
            {
                return;
            }

            IsCancelling = true;
            CancelCommand.RaiseCanExecuteChanged();

            EditorTaskBase task = _task;
            _ = EngineManager.PostToSimThread(() => task.Cancel());
        }

        public void Detach()
        {
            _descriptionChangedHandler?.Remove();
            _descriptionChangedHandler = null;
        }
    }
}
