using System;
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
    public record ModelImportSnapshot(
        string FileName,
        bool HasMesh,
        uint BoneCount,
        IReadOnlyList<(string Id, string Name, uint FrameCount)> Takes,
        IReadOnlyList<string> SkeletonNames,
        IReadOnlyList<ObjectBase> Settings);

    public class ImportSettingsSectionViewModel
    {
        public string Label { get; }
        public ComponentSubObjectViewModel Body { get; }

        public ImportSettingsSectionViewModel(string label, ComponentSubObjectViewModel body)
        {
            Label = label;
            Body = body;
        }
    }

    public class ModelImportTakeViewModel : ViewModelBase
    {
        private bool _isIncluded = true;

        public ModelImportTakeViewModel(string id, string name, uint frameCount)
        {
            Id = id;
            Name = name;
            FrameCount = frameCount;
        }

        public string Id { get; }

        public string Name { get; }

        public uint FrameCount { get; }

        public string FrameCountText => $"{FrameCount} frames";

        public bool IsIncluded
        {
            get => _isIncluded;
            set => SetProperty(ref _isIncluded, value);
        }
    }

    public class ModelImportSplitViewModel : ViewModelBase
    {
        private string _name;
        private string _take;
        private decimal _startFrame;
        private decimal _endFrame;
        private bool _loop = true;
        private bool _rootMotion;

        public ModelImportSplitViewModel(string name, string take, decimal endFrame, Action<ModelImportSplitViewModel> remove)
        {
            _name = name;
            _take = take;
            _endFrame = endFrame;

            RemoveCommand = new RelayCommand(() => remove(this));
        }

        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        public string Take
        {
            get => _take;
            set => SetProperty(ref _take, value);
        }

        public decimal StartFrame
        {
            get => _startFrame;
            set => SetProperty(ref _startFrame, Math.Max(Math.Round(value), 0));
        }

        public decimal EndFrame
        {
            get => _endFrame;
            set => SetProperty(ref _endFrame, Math.Max(Math.Round(value), 0));
        }

        public bool Loop
        {
            get => _loop;
            set => SetProperty(ref _loop, value);
        }

        public bool RootMotion
        {
            get => _rootMotion;
            set => SetProperty(ref _rootMotion, value);
        }

        public ICommand RemoveCommand { get; }
    }

    public class ModelImportPanelViewModel : EditorPanelViewModel
    {
        private const int PollIntervalMs = 500;

        private static readonly string[] RigModeNames = { "Create skeleton", "Use existing", "None" };

        private readonly EditorSubsystem _editorSubsystem;
        private readonly EditorModelImportReview _review;
        private readonly DispatcherTimer _pollTimer;
        private readonly Action _onFinished;

        private int _rigModeIndex;
        private Skeleton? _targetSkeletonAsset;
        private string? _targetSkeleton;
        private decimal _scale = 1.0m;
        private bool _generateMeshLods = true;
        private bool _autoMapHumanoid = true;
        private string _unmatchedTracksText = string.Empty;
        private bool _resolved;

        public ModelImportPanelViewModel(
            EditorSubsystem editorSubsystem,
            EditorModelImportReview review,
            ModelImportSnapshot snapshot,
            Action onFinished)
            : base($"Import {snapshot.FileName}")
        {
            _editorSubsystem = editorSubsystem;
            _review = review;
            _onFinished = onFinished;

            HasMesh = snapshot.HasMesh;
            BoneCountText = $"{snapshot.BoneCount} bones";

            foreach ((string id, string name, uint frameCount) in snapshot.Takes)
            {
                Takes.Add(new ModelImportTakeViewModel(id, name, frameCount));
            }

            // getter and setter run on the sim thread
            SkeletonPicker = new ObjectPropertyViewModel(
                "Skeleton",
                Class.GetClass<Skeleton>().TypeInfo,
                getter: () => new BoxedValue(_targetSkeletonAsset),
                setter: value =>
                {
                    _targetSkeletonAsset = value.GetValue() as Skeleton;

                    string? skeletonName = _targetSkeletonAsset != null && _targetSkeletonAsset.IsValid
                        ? _targetSkeletonAsset.Name.ToString()
                        : null;

                    Dispatcher.UIThread.Post(() => TargetSkeleton = skeletonName);
                },
                isReadOnly: false);

            SkeletonPicker.RefreshValue();

            foreach (ObjectBase settings in snapshot.Settings)
            {
                ClassAttribute? labelAttribute = settings.Class.GetAttribute("label");
                string label = labelAttribute.HasValue ? labelAttribute.Value.GetString() : settings.Class.Name.ToString();

                SettingsSections.Add(new ImportSettingsSectionViewModel(label, new ComponentSubObjectViewModel(label, settings, depth: 1)));
            }

            AddSplitCommand = new RelayCommand(AddSplit, () => Takes.Count > 0);
            ImportCommand = new RelayCommand(OnImport);
            CancelCommand = new RelayCommand(OnCancel);

            OnClosed = OnCancel;

            // another path (a bridge command, a second panel) can resolve the same import
            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            _pollTimer.Tick += (_, _) => PollFinished();
            _pollTimer.Start();
        }

        public IReadOnlyList<string> RigModes => RigModeNames;

        public ObjectPropertyViewModel SkeletonPicker { get; }

        public ObservableCollection<ModelImportTakeViewModel> Takes { get; } = new();

        public ObservableCollection<ModelImportSplitViewModel> Splits { get; } = new();

        public ObservableCollection<ImportSettingsSectionViewModel> SettingsSections { get; } = new();

        public bool HasMesh { get; }

        public bool HasTakes => Takes.Count > 0;

        public string BoneCountText { get; }

        public int RigModeIndex
        {
            get => _rigModeIndex;
            set
            {
                if (SetProperty(ref _rigModeIndex, value))
                {
                    OnPropertyChanged(nameof(IsUsingExistingSkeleton));
                    RefreshUnmatchedTracks();
                }
            }
        }

        public bool IsUsingExistingSkeleton => _rigModeIndex == 1;

        public string? TargetSkeleton
        {
            get => _targetSkeleton;
            private set
            {
                if (SetProperty(ref _targetSkeleton, value))
                {
                    RefreshUnmatchedTracks();
                }
            }
        }

        public string UnmatchedTracksText
        {
            get => _unmatchedTracksText;
            private set => SetProperty(ref _unmatchedTracksText, value);
        }

        public decimal Scale
        {
            get => _scale;
            set => SetProperty(ref _scale, Math.Max(value, 0.0001m));
        }

        public bool GenerateMeshLods
        {
            get => _generateMeshLods;
            set => SetProperty(ref _generateMeshLods, value);
        }

        public bool AutoMapHumanoid
        {
            get => _autoMapHumanoid;
            set => SetProperty(ref _autoMapHumanoid, value);
        }

        public ICommand AddSplitCommand { get; }
        public ICommand ImportCommand { get; }
        public ICommand CancelCommand { get; }

        private void AddSplit()
        {
            ModelImportTakeViewModel take = Takes[0];

            Splits.Add(new ModelImportSplitViewModel($"{take.Name}_{Splits.Count + 1}", take.Id, (decimal)take.FrameCount, split => Splits.Remove(split)));
        }

        private void RefreshUnmatchedTracks()
        {
            if (!IsUsingExistingSkeleton || string.IsNullOrEmpty(_targetSkeleton))
            {
                UnmatchedTracksText = string.Empty;

                return;
            }

            string targetSkeleton = _targetSkeleton;

            _ = EngineManager.PostToSimThread(() =>
            {
                uint numUnmatched = _review.CountUnmatchedTracks(new Name(targetSkeleton));

                Dispatcher.UIThread.Post(() => UnmatchedTracksText = numUnmatched == 0 ? "All tracks match" : $"{numUnmatched} unmatched tracks");
            });
        }

        private void PollFinished()
        {
            if (_resolved)
            {
                _pollTimer.Stop();

                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!_review.IsFinished())
                {
                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    if (!_resolved)
                    {
                        Close();
                    }
                });
            });
        }

        private void OnImport()
        {
            if (_resolved)
            {
                return;
            }

            uint rigMode = (uint)_rigModeIndex;
            string targetSkeleton = _targetSkeleton ?? string.Empty;
            float scale = (float)_scale;
            bool generateMeshLods = _generateMeshLods;
            bool autoMapHumanoid = _autoMapHumanoid;
            List<(string Id, bool Included)> takes = Takes.Select(take => (take.Id, take.IsIncluded)).ToList();
            List<ModelImportSplitViewModel> splits = Splits.ToList();

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _review.SetRigMode(rigMode);
                    _review.SetTargetSkeleton(new Name(targetSkeleton));
                    _review.SetScale(scale);
                    _review.SetGenerateMeshLods(generateMeshLods);
                    _review.SetAutoMapHumanoid(autoMapHumanoid);

                    foreach ((string id, bool included) in takes)
                    {
                        _review.SetTakeIncluded(new Name(id), included);
                    }

                    _review.ClearSplits();

                    foreach (ModelImportSplitViewModel split in splits)
                    {
                        _review.AddSplit(new Name(split.Name), new Name(split.Take), (uint)split.StartFrame, (uint)split.EndFrame, split.Loop, split.RootMotion);
                    }

                    _review.Commit(_editorSubsystem);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Model import failed: {ex.Message}");
                }
            });

            Close();
        }

        private void OnCancel()
        {
            if (_resolved)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() => _review.Cancel());

            Close();
        }

        private void Close()
        {
            _resolved = true;
            _pollTimer.Stop();

            foreach (ImportSettingsSectionViewModel section in SettingsSections)
            {
                section.Body.Dispose();
            }

            PanelService.Instance.RemovePanel(this);

            _onFinished();
        }
    }
}
