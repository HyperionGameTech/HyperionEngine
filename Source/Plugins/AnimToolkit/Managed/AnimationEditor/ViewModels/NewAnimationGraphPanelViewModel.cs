using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class NewAnimationGraphPanelViewModel : EditorPanelViewModel
    {
        private readonly string _folderPath;

        private string _graphName = "NewAnimationGraph";
        private string? _selectedSkeleton;
        private bool _isCreating;

        public NewAnimationGraphPanelViewModel(string folderPath)
            : base("New Animation Graph")
        {
            _folderPath = folderPath;

            CreateCommand = new RelayCommand(Create, () => CanCreate);
            CancelCommand = new RelayCommand(() => PanelService.Instance.RemovePanel(this));

            LoadSkeletons();
        }

        public ObservableCollection<string> SkeletonChoices { get; } = new ObservableCollection<string>();

        public ICommand CreateCommand { get; }
        public ICommand CancelCommand { get; }

        public string GraphName
        {
            get => _graphName;
            set
            {
                if (SetProperty(ref _graphName, value))
                {
                    OnPropertyChanged(nameof(CanCreate));
                    (CreateCommand as RelayCommand)?.RaiseCanExecuteChanged();
                }
            }
        }

        public string? SelectedSkeleton
        {
            get => _selectedSkeleton;
            set
            {
                if (SetProperty(ref _selectedSkeleton, value))
                {
                    OnPropertyChanged(nameof(CanCreate));
                    (CreateCommand as RelayCommand)?.RaiseCanExecuteChanged();
                }
            }
        }

        public bool CanCreate => !_isCreating && _graphName.Trim().Length > 0 && !string.IsNullOrEmpty(_selectedSkeleton);

        private void LoadSkeletons()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                EditorAnimationEditState? state = AnimationEditorWiring.GetState();

                if (state == null)
                {
                    return;
                }

                List<string> names = state.GetSkeletonAssetNames().Cast<Name>().Select(n => n.ToString()).OrderBy(n => n).ToList();
                string focused = state.GetFocusedSkeletonName().ToString();

                Dispatcher.UIThread.Post(() =>
                {
                    SkeletonChoices.Clear();

                    foreach (string name in names)
                    {
                        SkeletonChoices.Add(name);
                    }

                    if (names.Contains(focused))
                    {
                        SelectedSkeleton = focused;
                    }
                    else if (names.Count > 0)
                    {
                        SelectedSkeleton = names[0];
                    }
                });
            });
        }

        private void Create()
        {
            if (!CanCreate)
            {
                return;
            }

            _isCreating = true;
            OnPropertyChanged(nameof(CanCreate));

            string graphName = _graphName.Trim();
            string skeletonName = _selectedSkeleton!;
            string folderPath = _folderPath;

            _ = EngineManager.PostToSimThread(() =>
            {
                AnimationGraph? graph = AnimationEditorWiring.GetState()?.CreateGraphAsset(new Name(graphName), new Name(skeletonName));

                if (graph != null && graph.IsValid)
                {
                    EngineManager.CurrentProject?.AssetFolders?.SetAssetFolderOf(graph.Path.BucketIndex, graph.Name, folderPath);
                    MainWindowViewModel.Instance?.ContentBrowser.NotifyAssetsChanged();
                }

                Dispatcher.UIThread.Post(() =>
                {
                    PanelService.Instance.RemovePanel(this);

                    if (graph != null && graph.IsValid)
                    {
                        MainWindowViewModel.Instance?.RequestEditAsset(graph);
                    }
                });
            });
        }
    }
}
