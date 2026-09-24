using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class StartScreenViewModel : ViewModelBase, IDisposable
    {
        private readonly DelegateHandler _currentProjectChangedHandler;

        private RecentProjectItemViewModel? _lastProject;
        private bool _alwaysOpenLastProject;
        private bool _isBusy;
        private string _busyText = string.Empty;
        private bool _awaitingProjectOpen;
        private bool _isFinished;

        public ObservableCollection<RecentProjectItemViewModel> RecentProjects { get; } = new();

        public bool HasRecentProjects => RecentProjects.Count > 0;

        public RecentProjectItemViewModel? LastProject
        {
            get => _lastProject;
            private set
            {
                if (SetProperty(ref _lastProject, value))
                {
                    OnPropertyChanged(nameof(HasLastProject));
                }
            }
        }

        public bool HasLastProject => LastProject != null;

        public bool AlwaysOpenLastProject
        {
            get => _alwaysOpenLastProject;
            set
            {
                if (SetProperty(ref _alwaysOpenLastProject, value))
                {
                    RecentProjectsService.Instance.AlwaysOpenLastProject = value;
                }
            }
        }

        public bool IsBusy
        {
            get => _isBusy;
            private set => SetProperty(ref _isBusy, value);
        }

        public string BusyText
        {
            get => _busyText;
            private set => SetProperty(ref _busyText, value);
        }

        public ICommand NewProjectCommand { get; }
        public ICommand BrowseCommand { get; }
        public ICommand OpenLastProjectCommand { get; }
        public ICommand OpenRecentProjectCommand { get; }
        public ICommand RemoveRecentProjectCommand { get; }

        /// Raised once the user has picked something and the editor should take over
        public event Action? Finished;

        public StartScreenViewModel()
        {
            NewProjectCommand = new RelayCommand(NewProject, () => !IsBusy);
            BrowseCommand = new RelayCommand(Browse, () => !IsBusy);
            OpenLastProjectCommand = new RelayCommand(() => OpenProject(LastProject), () => !IsBusy && HasLastProject);
            OpenRecentProjectCommand = new RelayCommand<RecentProjectItemViewModel>(OpenProject, _ => !IsBusy);
            RemoveRecentProjectCommand = new RelayCommand<RecentProjectItemViewModel>(item =>
            {
                if (item != null)
                {
                    RecentProjectsService.Instance.Remove(item.FilePath);
                }
            });

            _alwaysOpenLastProject = RecentProjectsService.Instance.AlwaysOpenLastProject;

            RecentProjectsService.Instance.Changed += RefreshRecentProjects;
            RefreshRecentProjects();

            _currentProjectChangedHandler = EditorState.Instance.GetOnCurrentProjectChangedDelegate()
                .Bind((EditorProject? newProject, bool isSimulationStateChange) =>
                {
                    bool isSaved = newProject != null && newProject.IsSaved;

                    Dispatcher.UIThread.Post(() => OnCurrentProjectChanged(isSaved));
                });
        }

        public void Dispose()
        {
            RecentProjectsService.Instance.Changed -= RefreshRecentProjects;
            _currentProjectChangedHandler.Remove();
        }

        private void RefreshRecentProjects()
        {
            RecentProjects.Clear();

            foreach (string projectFilepath in RecentProjectsService.Instance.RecentProjects)
            {
                RecentProjects.Add(new RecentProjectItemViewModel(projectFilepath));
            }

            LastProject = RecentProjects.Count > 0 && RecentProjects[0].Exists ? RecentProjects[0] : null;

            OnPropertyChanged(nameof(HasRecentProjects));
            RaiseCanExecuteChanged();
        }

        private void NewProject()
        {
            // The engine starts up with a fresh untitled project, so there is normally nothing to create
            if (EngineManager.CurrentProject?.IsSaved == true)
            {
                RecentProjectsService.Instance.NewProject();
            }

            Finish();
        }

        private void Browse()
        {
            // The dialog's result arrives through the current project changing; cancelling it leaves the start screen up
            _awaitingProjectOpen = true;

            RecentProjectsService.Instance.BrowseForProject();
        }

        private void OpenProject(RecentProjectItemViewModel? item)
        {
            if (item == null)
            {
                return;
            }

            // A missing project is reported and dropped from the list by the engine, and nothing gets opened
            if (item.Exists)
            {
                _awaitingProjectOpen = true;

                BusyText = $"Opening {item.Name}...";
                IsBusy = true;
                RaiseCanExecuteChanged();
            }

            RecentProjectsService.Instance.OpenProject(item.FilePath);
        }

        private void OnCurrentProjectChanged(bool isSaved)
        {
            if (!_awaitingProjectOpen)
            {
                return;
            }

            if (isSaved)
            {
                Finish();

                return;
            }

            // Opening failed and the engine fell back to a new project
            IsBusy = false;
            RaiseCanExecuteChanged();
        }

        private void Finish()
        {
            if (_isFinished)
            {
                return;
            }

            _isFinished = true;
            _awaitingProjectOpen = false;

            Finished?.Invoke();
        }

        private void RaiseCanExecuteChanged()
        {
            ((RelayCommand)NewProjectCommand).RaiseCanExecuteChanged();
            ((RelayCommand)BrowseCommand).RaiseCanExecuteChanged();
            ((RelayCommand)OpenLastProjectCommand).RaiseCanExecuteChanged();
            ((RelayCommand<RecentProjectItemViewModel>)OpenRecentProjectCommand).RaiseCanExecuteChanged();
        }
    }
}
