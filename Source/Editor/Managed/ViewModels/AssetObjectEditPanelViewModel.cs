using System;
using System.ComponentModel;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>
    /// Pop-out editor for the object held by an <see cref="ObjectPropertyViewModel"/>. It follows the
    /// source property rather than snapshotting its sub-object, so re-assigning the asset re-targets
    /// the panel (and clearing it closes the panel) instead of leaving it editing a detached object.
    /// </summary>
    public class AssetObjectEditPanelViewModel : EditorPanelViewModel
    {
        private readonly ObjectPropertyViewModel _source;

        public string Heading { get; }

        private ComponentSubObjectViewModel? _subObject;
        public ComponentSubObjectViewModel? SubObject
        {
            get => _subObject;
            private set => SetProperty(ref _subObject, value);
        }

        private string _subHeading = string.Empty;
        public string SubHeading
        {
            get => _subHeading;
            private set => SetProperty(ref _subHeading, value);
        }

        private bool _hasSubHeading;
        public bool HasSubHeading
        {
            get => _hasSubHeading;
            private set => SetProperty(ref _hasSubHeading, value);
        }

        public AssetObjectEditPanelViewModel(ObjectPropertyViewModel source)
            : base($"Edit {source?.Label}")
        {
            _source = source ?? throw new ArgumentNullException(nameof(source));

            Heading = source.Label;

            _source.PropertyChanged += OnSourcePropertyChanged;
            OnClosed = () => _source.PropertyChanged -= OnSourcePropertyChanged;

            SyncFromSource();
        }

        private void OnSourcePropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName != nameof(ObjectPropertyViewModel.SubObject)
                && e.PropertyName != nameof(ObjectPropertyViewModel.AssetPathDisplay))
            {
                return;
            }

            SyncFromSource();
        }

        private void SyncFromSource()
        {
            SubObject = _source.SubObject;

            string assetPath = _source.AssetPathDisplay;
            bool hasPath = !string.IsNullOrEmpty(assetPath) && assetPath != "(None)";

            SubHeading = hasPath ? assetPath : string.Empty;
            HasSubHeading = hasPath;

            if (SubObject == null && ReferenceEquals(PanelService.Instance.ActivePanel, this))
            {
                // The property no longer points at anything - there is nothing left to edit.
                PanelService.Instance.ClosePanel();
            }
        }
    }
}
