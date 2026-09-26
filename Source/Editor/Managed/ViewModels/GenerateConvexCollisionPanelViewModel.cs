using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class GenerateConvexCollisionPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorSubsystem _editorSubsystem;
        private readonly ICommand _generateCommand;
        private readonly string _nodeUuid;

        private readonly InspectorPropertyViewModelBase? _settingsProperty;

        public string TargetName { get; }

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; }

        public ObservableCollection<string> Presets { get; } = new ObservableCollection<string>();

        private int _selectedPresetIndex = -1;
        public int SelectedPresetIndex
        {
            get => _selectedPresetIndex;
            set
            {
                if (SetProperty(ref _selectedPresetIndex, value) && value >= 0)
                {
                    ApplyPreset((uint)value);
                }
            }
        }

        public ICommand GenerateCommand { get; }
        public ICommand CloseCommand { get; }

        public GenerateConvexCollisionPanelViewModel(EditorSubsystem editorSubsystem, NodeViewModel node, IReadOnlyList<string> presetNames, ICommand generateCommand)
            : base("Generate Convex Collision")
        {
            ArgumentNullException.ThrowIfNull(node);

            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));
            _generateCommand = generateCommand ?? throw new ArgumentNullException(nameof(generateCommand));
            _nodeUuid = node.UUID.ToString();

            TargetName = node.DisplayName;

            foreach (string presetName in presetNames)
            {
                Presets.Add(presetName);
            }

            Property? settingsProperty = editorSubsystem.Class.GetProperty(new Name("ConvexCollisionSettings"));

            if (settingsProperty != null)
            {
                _settingsProperty = InspectorViewModelFactory.Create(
                    editorSubsystem,
                    settingsProperty.Value,
                    isReadOnly: false,
                    valueChangedCallback: () => SelectedPresetIndex = -1);
            }
            else
            {
                Logger.Log(LogLevel.Error, "Generate Convex Collision: EditorSubsystem has no ConvexCollisionSettings property");
            }

            Properties = (_settingsProperty as StructPropertyViewModel)?.SubProperties
                ?? new ObservableCollection<InspectorPropertyViewModelBase>();

            GenerateCommand = new RelayCommand(OnGenerate);
            CloseCommand = new RelayCommand(() => PanelService.Instance.RemovePanel(this));

            Node targetNode = node.Node;

            _ = EngineManager.PostToSimThread(() => _editorSubsystem.SetConvexCollisionTarget(targetNode));

            OnClosed = () =>
            {
                _ = EngineManager.PostToSimThread(() => _editorSubsystem.SetConvexCollisionTarget(null));
            };
        }

        private void ApplyPreset(uint presetIndex)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                _editorSubsystem.ApplyConvexCollisionPreset(presetIndex);

                Dispatcher.UIThread.Post(() => _settingsProperty?.RefreshValue());
            });
        }

        private void OnGenerate()
        {
            if (!_generateCommand.CanExecute(_nodeUuid))
            {
                Logger.Log(LogLevel.Warning, "Cannot generate convex collision while simulation is active.");

                return;
            }

            _generateCommand.Execute(_nodeUuid);
        }
    }
}
