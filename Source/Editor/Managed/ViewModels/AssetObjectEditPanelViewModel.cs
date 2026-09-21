using System;
using System.ComponentModel;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>
    /// Dynamic panel shown used to edit an asset object's properties
    /// </summary>
    public class AssetObjectEditPanelViewModel : EditorPanelViewModel
    {
        private readonly ObjectPropertyViewModel _source;

        public string Heading { get; }

        /// <summary>Duplicates the edited asset and reassigns the source property to point to the clone.</summary>
        public ICommand CloneCommand => _source.CloneCommand;
        public bool CanClone => _source.CanClone;

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

        private MaterialPreviewViewModel? _preview;

        /// <summary>Live preview of the edited asset, or null for asset types that have no preview.</summary>
        public MaterialPreviewViewModel? Preview
        {
            get => _preview;
            private set
            {
                if (SetProperty(ref _preview, value))
                {
                    OnPropertyChanged(nameof(HasPreview));
                }
            }
        }

        public bool HasPreview => _preview != null;

        /// <summary>
        /// The panel may be opened on a property that already reports edits to its owner (the inspector
        /// does this), so the existing callback is kept and chained rather than replaced.
        /// </summary>
        private readonly Action? _previousValueChangedCallback;

        public AssetObjectEditPanelViewModel(ObjectPropertyViewModel source, Action? onClosed = null)
            : base($"Edit {source?.Label}")
        {
            _source = source ?? throw new ArgumentNullException(nameof(source));

            Heading = source.Label;

            _previousValueChangedCallback = _source.ValueChangedCallback;
            _source.ValueChangedCallback = () =>
            {
                _previousValueChangedCallback?.Invoke();

                // The edit has already been written to the material, so the next rendered frame shows it.
                _preview?.Invalidate();
            };

            _source.PropertyChanged += OnSourcePropertyChanged;
            OnClosed = () =>
            {
                _source.PropertyChanged -= OnSourcePropertyChanged;
                _source.ValueChangedCallback = _previousValueChangedCallback;

                Preview?.Dispose();
                Preview = null;

                onClosed?.Invoke();
            };

            SyncFromSource();
        }

        private void OnSourcePropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ObjectPropertyViewModel.CanClone))
            {
                OnPropertyChanged(nameof(CanClone));
                return;
            }

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

            SyncPreview();

            if (SubObject == null)
            {
                PanelService.Instance.RemovePanel(this);
            }
        }

        /// <summary>Creates or tears down the preview to match whatever the panel is currently editing.</summary>
        private void SyncPreview()
        {
            // Only materials have a preview today; everything else keeps a properties-only panel.
            if (SubObject?.Target is not Material material || !material.IsRegistered())
            {
                Preview?.Dispose();
                Preview = null;

                return;
            }

            AssetPath path = material.Path;

            if (Preview != null)
            {
                // Already previewing something; only rebuild when the panel moved to a different asset.
                if (Preview.Matches(path.BucketIndex, material.Name))
                {
                    return;
                }

                Preview.Dispose();
                Preview = null;
            }

            EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

            if (editorSubsystem == null)
            {
                return;
            }

            Preview = new MaterialPreviewViewModel(editorSubsystem, path.BucketIndex, material.Name);
        }
    }
}
