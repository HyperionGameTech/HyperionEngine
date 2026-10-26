using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class AssetExtensionSectionViewModel : ViewModelBase
    {
        public string Label { get; }
        public ComponentSubObjectViewModel Body { get; }
        public ICommand RemoveCommand { get; }

        public AssetExtensionSectionViewModel(string label, ComponentSubObjectViewModel body, Action remove)
        {
            Label = label;
            Body = body;
            RemoveCommand = new RelayCommand(remove);
        }
    }

    public class AddAssetExtensionOptionViewModel
    {
        public string Label { get; }
        public ICommand AddCommand { get; }

        public AddAssetExtensionOptionViewModel(string label, Action add)
        {
            Label = label;
            AddCommand = new RelayCommand(add);
        }
    }

    /// <summary>The extension sections of an asset's edit panel, and the extensions it can still have added.</summary>
    public class AssetExtensionsViewModel : ViewModelBase
    {
        private readonly AssetObject _asset;

        public ObservableCollection<AssetExtensionSectionViewModel> Sections { get; } = new();
        public ObservableCollection<AddAssetExtensionOptionViewModel> AddableExtensions { get; } = new();

        private bool _hasAddableExtensions;
        public bool HasAddableExtensions
        {
            get => _hasAddableExtensions;
            private set => SetProperty(ref _hasAddableExtensions, value);
        }

        public AssetExtensionsViewModel(AssetObject asset)
        {
            _asset = asset ?? throw new ArgumentNullException(nameof(asset));

            Refresh();
        }

        public void Dispose()
        {
            foreach (AssetExtensionSectionViewModel section in Sections)
            {
                section.Body.Dispose();
            }

            Sections.Clear();
        }

        public void Refresh()
        {
            Dispatcher.UIThread.VerifyAccess();

            Dispose();
            AddableExtensions.Clear();

            if (!_asset.IsValid)
            {
                HasAddableExtensions = false;
                return;
            }

            foreach (Name className in _asset.GetExtensionClassNames())
            {
                AssetExtension? extension = _asset.GetOrAddExtensionOfClass(className);

                if (extension == null)
                {
                    continue;
                }

                string label = GetLabel(className);
                Name capturedClassName = className;

                ComponentSubObjectViewModel body = new ComponentSubObjectViewModel(label, extension, depth: 1, postWriteCallback: () => extension.NotifyChanged());

                Sections.Add(new AssetExtensionSectionViewModel(label, body, () => Modify(() => _asset.RemoveExtensionOfClass(capturedClassName))));
            }

            foreach (Name className in _asset.GetAddableExtensionClasses())
            {
                Name capturedClassName = className;

                AddableExtensions.Add(new AddAssetExtensionOptionViewModel(GetLabel(className), () => Modify(() => _asset.AddExtensionOfClass(capturedClassName))));
            }

            HasAddableExtensions = AddableExtensions.Count > 0;
        }

        private void Modify(Action change)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    change();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to change the extensions of asset '{_asset.Name}': {ex.Message}");
                }

                Dispatcher.UIThread.Post(Refresh);
            });
        }

        private static string GetLabel(Name className)
        {
            ClassAttribute? attrLabel = Class.TryGetClass(className.ToString())?.GetAttribute("label");

            return attrLabel.HasValue ? attrLabel.Value.GetString() : className.ToString();
        }
    }
}
