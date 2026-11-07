using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class StartScreenViewModel : ViewModelBase, IDisposable
    {
        private const int SearchThreshold = 8;

        private readonly DelegateHandler _currentProjectChangedHandler;
        private readonly DelegateHandler _projectLoadStatusChangedHandler;

        private readonly List<RecentProjectItemViewModel> _allRecentProjects = new();
        private RecentProjectItemViewModel? _selectedProject;
        private string _filterText = string.Empty;
        private bool _alwaysOpenLastProject;
        private bool _isBusy;
        private string _busyText = string.Empty;
        private string _loadStatusText = string.Empty;
        private bool _awaitingProjectOpen;
        private bool _awaitingNewProject;
        private bool _isCreatingProject;
        private bool _isBrowsing;
        private bool _isFinished;

        public ObservableCollection<RecentProjectItemViewModel> RecentProjects { get; } = new();

        public bool HasRecentProjects => _allRecentProjects.Count > 0;

        public bool IsSearchVisible => _allRecentProjects.Count > SearchThreshold;

        public RecentProjectItemViewModel? SelectedProject
        {
            get => _selectedProject;
            set => SetProperty(ref _selectedProject, value);
        }

        public string FilterText
        {
            get => _filterText;
            set
            {
                if (SetProperty(ref _filterText, value))
                {
                    ApplyFilter();
                }
            }
        }

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

        public bool IsCreatingProject
        {
            get => _isCreatingProject;
            private set
            {
                if (SetProperty(ref _isCreatingProject, value))
                {
                    RaiseCanExecuteChanged();
                }
            }
        }

        public bool IsBrowsing
        {
            get => _isBrowsing;
            private set
            {
                if (SetProperty(ref _isBrowsing, value))
                {
                    RaiseCanExecuteChanged();
                }
            }
        }

        private bool CanStartAction => !IsBusy && !IsCreatingProject && !IsBrowsing;

        public string BusyText
        {
            get => _busyText;
            private set => SetProperty(ref _busyText, value);
        }

        public string LoadStatusText
        {
            get => _loadStatusText;
            private set => SetProperty(ref _loadStatusText, value);
        }

        public ICommand NewProjectCommand { get; }
        public ICommand BrowseCommand { get; }
        public ICommand OpenRecentProjectCommand { get; }
        public ICommand RemoveRecentProjectCommand { get; }
        public ICommand ShowInExplorerCommand { get; }
        public ICommand TogglePinCommand { get; }

        /// Raised once the user has picked something and the editor should take over
        public event Action? Finished;

        public StartScreenViewModel()
        {
            NewProjectCommand = new RelayCommand(NewProject, () => CanStartAction);
            BrowseCommand = new RelayCommand(Browse, () => CanStartAction);
            OpenRecentProjectCommand = new RelayCommand<RecentProjectItemViewModel>(OpenProject, _ => CanStartAction);
            RemoveRecentProjectCommand = new RelayCommand<RecentProjectItemViewModel>(item =>
            {
                if (item != null)
                {
                    RecentProjectsService.Instance.Remove(item.FilePath);
                }
            });
            ShowInExplorerCommand = new RelayCommand<RecentProjectItemViewModel>(ShowInExplorer);
            TogglePinCommand = new RelayCommand<RecentProjectItemViewModel>(item =>
            {
                if (item != null)
                {
                    RecentProjectsService.Instance.SetPinned(item.FilePath, !item.IsPinned);
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

            _projectLoadStatusChangedHandler = EditorState.Instance.GetOnProjectLoadStatusChangedDelegate()
                .Bind((string status, uint percent) =>
                {
                    Dispatcher.UIThread.Post(() => OnProjectLoadStatusChanged(status, percent));
                });
        }

        public void Dispose()
        {
            RecentProjectsService.Instance.Changed -= RefreshRecentProjects;
            _currentProjectChangedHandler.Remove();
            _projectLoadStatusChangedHandler.Remove();
        }

        private void RefreshRecentProjects()
        {
            if (_awaitingNewProject && EngineManager.CurrentProject?.IsSaved == true)
            {
                Finish();

                return;
            }

            string? selectedFilePath = SelectedProject?.FilePath;

            _allRecentProjects.Clear();

            foreach (RecentProjectEntry entry in RecentProjectsService.Instance.RecentProjectEntries)
            {
                _allRecentProjects.Add(new RecentProjectItemViewModel(entry));
            }

            ApplyFilter(selectedFilePath);

            OnPropertyChanged(nameof(HasRecentProjects));
            OnPropertyChanged(nameof(IsSearchVisible));
            RaiseCanExecuteChanged();
        }

        private void ApplyFilter(string? selectedFilePath = null)
        {
            selectedFilePath ??= SelectedProject?.FilePath;

            RecentProjects.Clear();

            foreach (RecentProjectItemViewModel item in _allRecentProjects.Where(item => item.Matches(FilterText)))
            {
                RecentProjects.Add(item);
            }

            SelectedProject = RecentProjects.FirstOrDefault(item => item.FilePath == selectedFilePath)
                ?? RecentProjects.Where(item => item.Exists).MaxBy(item => item.LastOpened ?? DateTime.MinValue)
                ?? RecentProjects.FirstOrDefault();
        }

        private void NewProject()
        {
            if (!CanStartAction)
            {
                return;
            }

            _awaitingNewProject = true;
            IsCreatingProject = true;

            RecentProjectsService.Instance.NewProject(() =>
            {
                _awaitingNewProject = false;
                IsCreatingProject = false;
            });
        }

        private void Browse()
        {
            // The dialog's result arrives through the current project changing; cancelling it leaves the start screen up
            if (!CanStartAction)
            {
                return;
            }

            _awaitingProjectOpen = true;
            IsBrowsing = true;

            RecentProjectsService.Instance.BrowseForProject(() => IsBrowsing = false);
        }

        private void OpenProject(RecentProjectItemViewModel? item)
        {
            if (item == null || !CanStartAction)
            {
                return;
            }

            // A missing project is reported and dropped from the list by the engine, and nothing gets opened
            if (item.Exists)
            {
                _awaitingProjectOpen = true;

                BusyText = $"Opening {item.Name}";
                LoadStatusText = string.Empty;
                IsBusy = true;
                RaiseCanExecuteChanged();
            }

            RecentProjectsService.Instance.OpenProject(item.FilePath);
        }

        private static void ShowInExplorer(RecentProjectItemViewModel? item)
        {
            if (item == null)
            {
                return;
            }

            ShellService.RevealInFileManager(item.Exists ? item.FilePath : item.Directory);
        }

        private void OnCurrentProjectChanged(bool isSaved)
        {
            // a project opened from anywhere (this screen, an editor command) takes the editor past the start screen
            if (isSaved)
            {
                Finish();

                return;
            }

            if (!_awaitingProjectOpen)
            {
                return;
            }

            // Opening failed and the engine fell back to a new project
            IsBusy = false;
            RaiseCanExecuteChanged();
        }

        private void OnProjectLoadStatusChanged(string status, uint percent)
        {
            if (!_awaitingProjectOpen || _isFinished)
            {
                return;
            }

            if (!IsBusy)
            {
                BusyText = "Opening project";
                IsBusy = true;
                RaiseCanExecuteChanged();
            }

            LoadStatusText = $"{status}... {percent}%";
        }

        private void Finish()
        {
            if (_isFinished)
            {
                return;
            }

            _isFinished = true;
            _awaitingProjectOpen = false;
            _awaitingNewProject = false;

            Finished?.Invoke();
        }

        private void RaiseCanExecuteChanged()
        {
            ((RelayCommand)NewProjectCommand).RaiseCanExecuteChanged();
            ((RelayCommand)BrowseCommand).RaiseCanExecuteChanged();
            ((RelayCommand<RecentProjectItemViewModel>)OpenRecentProjectCommand).RaiseCanExecuteChanged();
        }
    }
}
