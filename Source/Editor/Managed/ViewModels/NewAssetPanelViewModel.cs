using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class NewAssetPanelViewModel : EditorPanelViewModel
    {
        private readonly Func<string, AssetObject?> _createInstance;
        private readonly Action<AssetObject?> _onCompleted;

        private AssetObject? _instance;
        private int _instanceVersion;

        public IReadOnlyList<ClassPickerEntry> ClassEntries { get; }

        public bool HasClassChoice { get; }

        private string _selectedClassName = string.Empty;
        public string SelectedClassName
        {
            get => _selectedClassName;
            set
            {
                if (SetProperty(ref _selectedClassName, value) && value.Length != 0)
                {
                    RebuildInstance(value);
                }
            }
        }

        private ComponentSubObjectViewModel? _assetSubObject;
        public ComponentSubObjectViewModel? AssetSubObject
        {
            get => _assetSubObject;
            private set => SetProperty(ref _assetSubObject, value);
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public NewAssetPanelViewModel(
            IReadOnlyList<ClassPickerEntry> classEntries,
            Func<string, AssetObject?> createInstance,
            Action<AssetObject?> onCompleted)
            : base("New Asset")
        {
            _createInstance = createInstance ?? throw new ArgumentNullException(nameof(createInstance));
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ClassEntries = classEntries;
            HasClassChoice = classEntries.Count(entry => !entry.IsAbstract) > 1;

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);

            SelectedClassName = classEntries.FirstOrDefault(entry => !entry.IsAbstract)?.Name ?? string.Empty;
        }

        private void RebuildInstance(string className)
        {
            int version = ++_instanceVersion;

            _ = EngineManager.PostToSimThread(() =>
            {
                AssetObject? instance = null;
                ComponentSubObjectViewModel? subObject = null;

                try
                {
                    instance = _createInstance(className);

                    if (instance != null)
                    {
                        subObject = new ComponentSubObjectViewModel(className, instance);
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to create a new '{className}': {ex.Message}");
                }

                Dispatcher.UIThread.Post(() =>
                {
                    if (version != _instanceVersion)
                    {
                        return;
                    }

                    _instance = instance;
                    AssetSubObject = subObject;
                });
            });
        }

        private void OnConfirm()
        {
            if (_instance == null)
            {
                return;
            }

            _onCompleted(_instance);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
