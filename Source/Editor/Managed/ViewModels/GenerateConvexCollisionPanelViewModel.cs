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
        private readonly Action _onGenerate;

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

        /// <summary>Targets a scene node: generating assigns convex collision to it.</summary>
        public GenerateConvexCollisionPanelViewModel(EditorSubsystem editorSubsystem, NodeViewModel node, IReadOnlyList<string> presetNames, ICommand generateCommand)
            : this(editorSubsystem, node?.DisplayName ?? throw new ArgumentNullException(nameof(node)), presetNames, MakeNodeGenerateAction(generateCommand, node))
        {
            Node targetNode = node.Node;

            _ = EngineManager.PostToSimThread(() => _editorSubsystem.SetConvexCollisionTarget(targetNode));

            OnClosed = () =>
            {
                _ = EngineManager.PostToSimThread(() => _editorSubsystem.SetConvexCollisionTarget(null));
            };
        }

        public GenerateConvexCollisionPanelViewModel(EditorSubsystem editorSubsystem, string targetName, IReadOnlyList<string> presetNames, Action onGenerate)
            : base("Generate Convex Collision")
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));
            _onGenerate = onGenerate ?? throw new ArgumentNullException(nameof(onGenerate));

            TargetName = targetName;

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

            GenerateCommand = new RelayCommand(_onGenerate);
            CloseCommand = new RelayCommand(() => PanelService.Instance.RemovePanel(this));
        }

        private static Action MakeNodeGenerateAction(ICommand generateCommand, NodeViewModel node)
        {
            ArgumentNullException.ThrowIfNull(generateCommand);

            string nodeUuid = node.UUID.ToString();

            return () =>
            {
                if (!generateCommand.CanExecute(nodeUuid))
                {
                    Logger.Log(LogLevel.Warning, "Cannot generate convex collision while simulation is active.");

                    return;
                }

                generateCommand.Execute(nodeUuid);
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
    }
}
