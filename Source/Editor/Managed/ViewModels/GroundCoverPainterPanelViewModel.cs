using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class GroundCoverPainterPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorTerrainState _terrainState;

        private readonly PainterAssetPicker _prefabPicker;

        public ObservableCollection<InspectorPropertyViewModelBase> Properties => _prefabPicker.Properties;

        public ObservableCollection<string> Layers { get; } = new ObservableCollection<string>();

        private string? _selectedLayer;
        public string? SelectedLayer
        {
            get => _selectedLayer;
            set
            {
                if (SetProperty(ref _selectedLayer, value) && !string.IsNullOrEmpty(value) && !_isRefreshingLayers)
                {
                    _terrainState.SetPaintGroundCoverLayer(new Name(value));

                    // queued behind the change, so the prefab picker shows what the layer plants
                    _ = EngineManager.PostToSimThread(() => _prefabPicker.Refresh());
                }
            }
        }

        public bool HasLayers => Layers.Count != 0;

        private bool _isRefreshingLayers;

        private double _radius;
        public double Radius
        {
            get => _radius;
            set
            {
                if (SetProperty(ref _radius, value))
                {
                    _terrainState.SetRadius((float)value);
                    OnPropertyChanged(nameof(RadiusText));
                }
            }
        }

        public string RadiusText => $"{_radius:0.#} m";

        private double _strength;
        public double Strength
        {
            get => _strength;
            set
            {
                if (SetProperty(ref _strength, value))
                {
                    _terrainState.SetStrength((float)value);
                    OnPropertyChanged(nameof(StrengthText));
                }
            }
        }

        public string StrengthText => $"{_strength:0.00}";

        public GroundCoverPainterPanelViewModel(EditorSubsystem? editorSubsystem, EditorTerrainState? terrainState, Action? onClosed)
            : base("Ground Cover Painter")
        {
            ArgumentNullException.ThrowIfNull(editorSubsystem);
            ArgumentNullException.ThrowIfNull(terrainState);

            _terrainState = terrainState;

            _radius = terrainState.Radius;
            _strength = terrainState.Strength;

            _prefabPicker = new PainterAssetPicker(
                Title, terrainState, "ActiveGroundCoverPrefab", AssetBucket.Prefabs, editorSubsystem,
                RefreshLayers, AssetBucket.Terrain);

            _prefabPicker.AssetBucketChanged += RefreshLayers;

            OnClosed = () =>
            {
                _prefabPicker.Dispose();

                onClosed?.Invoke();
            };

            RefreshLayers();
        }

        private void RefreshLayers()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                var layerNames = new List<string>();

                foreach (Name layerName in _terrainState.GetPaintableGroundCoverLayers())
                {
                    layerNames.Add(layerName.ToString());
                }

                string currentLayer = _terrainState.GetPaintGroundCoverLayer().ToString();
                bool hasActivePrefab = _terrainState.GetActiveGroundCoverPrefab() != null;

                Dispatcher.UIThread.Post(() =>
                {
                    _isRefreshingLayers = true;

                    Layers.Clear();

                    foreach (string layerName in layerNames)
                    {
                        Layers.Add(layerName);
                    }

                    OnPropertyChanged(nameof(HasLayers));

                    // a picked prefab with no layer yet leaves the list unselected until the first stroke adds it
                    SelectedLayer = layerNames.Contains(currentLayer) ? currentLayer : null;

                    _isRefreshingLayers = false;

                    if (SelectedLayer == null && !hasActivePrefab && layerNames.Count != 0)
                    {
                        SelectedLayer = layerNames[0];
                    }
                });
            });
        }
    }
}
