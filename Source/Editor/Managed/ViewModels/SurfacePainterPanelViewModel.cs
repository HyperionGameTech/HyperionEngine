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
    public abstract class SurfacePainterPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorSurfacePainterState _painterState;

        private readonly PainterAssetPicker _assetPicker;

        public string AssetLabel { get; }

        public virtual bool HasGroundFitting => false;

        public virtual bool HasPalette => false;

        public ObservableCollection<InspectorPropertyViewModelBase> Properties => _assetPicker.Properties;

        private double _scale;
        public double Scale
        {
            get => _scale;
            set
            {
                if (SetProperty(ref _scale, value))
                {
                    _painterState.SetScale((float)value);
                    OnPropertyChanged(nameof(ScaleText));
                }
            }
        }

        public string ScaleText => $"{_scale:0.00}x";

        private double _rotationDegrees;
        public double RotationDegrees
        {
            get => _rotationDegrees;
            set
            {
                if (SetProperty(ref _rotationDegrees, value))
                {
                    _painterState.SetRotationDegrees((float)value);
                    OnPropertyChanged(nameof(RotationText));
                }
            }
        }

        public string RotationText => $"{_rotationDegrees:0}°";

        private bool _randomRotation;
        public bool RandomRotation
        {
            get => _randomRotation;
            set
            {
                if (SetProperty(ref _randomRotation, value))
                {
                    _painterState.SetRandomRotation(value);
                }
            }
        }

        private double _spacing;
        public double Spacing
        {
            get => _spacing;
            set
            {
                if (SetProperty(ref _spacing, value))
                {
                    _painterState.SetSpacing((float)value);
                    OnPropertyChanged(nameof(SpacingText));
                }
            }
        }

        public string SpacingText => _spacing <= 0.0 ? "Single stamp" : $"{_spacing:0.0#} m";

        private double _eraseRadius;
        public double EraseRadius
        {
            get => _eraseRadius;
            set
            {
                if (SetProperty(ref _eraseRadius, value))
                {
                    _painterState.SetEraseRadius((float)value);
                    OnPropertyChanged(nameof(EraseRadiusText));
                }
            }
        }

        public string EraseRadiusText => $"{_eraseRadius:0.0#} m";

        private bool _alignToSurface;
        public bool AlignToSurface
        {
            get => _alignToSurface;
            set
            {
                if (SetProperty(ref _alignToSurface, value))
                {
                    _painterState.SetAlignToSurface(value);
                }
            }
        }

        private bool _eraseMode;

        public bool PaintMode
        {
            get => !_eraseMode;
            set
            {
                if (value)
                {
                    SetEraseMode(false);
                }
            }
        }

        public bool EraseMode
        {
            get => _eraseMode;
            set
            {
                if (value)
                {
                    SetEraseMode(true);
                }
            }
        }

        private void SetEraseMode(bool eraseMode)
        {
            if (_eraseMode == eraseMode)
            {
                return;
            }

            _eraseMode = eraseMode;
            _painterState.SetEraseMode(eraseMode);

            OnPropertyChanged(nameof(PaintMode));
            OnPropertyChanged(nameof(EraseMode));
        }

        protected SurfacePainterPanelViewModel(
            string title,
            string assetLabel,
            string activeAssetPropertyName,
            AssetBucket assetBucket,
            EditorSubsystem? editorSubsystem,
            EditorSurfacePainterState? painterState,
            Action? onClosed)
            : base(title)
        {
            ArgumentNullException.ThrowIfNull(editorSubsystem);
            ArgumentNullException.ThrowIfNull(painterState);

            _painterState = painterState;

            AssetLabel = assetLabel;

            _scale = painterState.Scale;
            _rotationDegrees = painterState.RotationDegrees;
            _randomRotation = painterState.RandomRotation;
            _spacing = painterState.Spacing;
            _eraseRadius = painterState.EraseRadius;
            _alignToSurface = painterState.AlignToSurface;
            _eraseMode = painterState.EraseMode;

            _assetPicker = new PainterAssetPicker(title, painterState, activeAssetPropertyName, assetBucket, editorSubsystem);
            _assetPicker.AssetBucketChanged += OnAssetBucketChanged;

            OnClosed = () =>
            {
                _assetPicker.Dispose();

                onClosed?.Invoke();
            };
        }

        /// <summary>Sim thread. Called when the painter's asset bucket changes, before the picker refreshes.</summary>
        protected virtual void OnAssetBucketChanged()
        {
        }

        protected HashSet<string> GetAssetNames()
        {
            return _assetPicker.GetAssetNames();
        }
    }

    public class DecalPainterPanelViewModel : SurfacePainterPanelViewModel
    {
        private readonly EditorDecalPainterState _decalPainterState;

        private HashSet<string>? _decalNamesBeforeCreate;

        public ICommand NewDecalCommand { get; }

        public DecalPainterPanelViewModel(EditorSubsystem? editorSubsystem, EditorDecalPainterState? painterState, ICommand? newDecalCommand, Action? onClosed)
            : base("Decal Painter", "Decal", "ActiveDecal", AssetBucket.Decals, editorSubsystem, painterState, onClosed)
        {
            ArgumentNullException.ThrowIfNull(newDecalCommand);

            _decalPainterState = painterState!;

            // same command as the content browser's New > Decal
            NewDecalCommand = new RelayCommand(() =>
            {
                // queued ahead of the command's own sim thread work, so this sees the bucket before the new asset lands
                _ = EngineManager.PostToSimThread(() =>
                {
                    _decalNamesBeforeCreate = GetAssetNames();
                });

                newDecalCommand.Execute(null);
            }, () => newDecalCommand.CanExecute(null));
        }

        protected override void OnAssetBucketChanged()
        {
            if (_decalNamesBeforeCreate == null)
            {
                return;
            }

            string? createdName = GetAssetNames().FirstOrDefault(name => !_decalNamesBeforeCreate.Contains(name));

            if (createdName != null)
            {
                _decalNamesBeforeCreate = null;

                _decalPainterState.SetActiveDecalByName(new Name(createdName));
            }
        }
    }

    public sealed class PaintSourceItem
    {
        public string Name { get; }
        public bool IsSet { get; }

        public string IconKind => IsSet ? "Layers" : "Prefab";
        public string KindText => IsSet ? "Set" : "Prefab";

        public PaintSourceItem(string name, bool isSet)
        {
            Name = name;
            IsSet = isSet;
        }

        public override string ToString() => Name;
    }

    public sealed class PaletteTileViewModel : ViewModelBase
    {
        private readonly Action<PaletteTileViewModel> _onClick;

        private Action<Avalonia.Media.IImage>? _thumbnailCallback;

        public uint Index { get; }
        public string Name { get; }
        public string WeightText { get; }
        public bool HasWeight => !string.IsNullOrEmpty(WeightText);

        private bool _isIncluded;
        public bool IsIncluded
        {
            get => _isIncluded;
            set
            {
                if (SetProperty(ref _isIncluded, value))
                {
                    OnPropertyChanged(nameof(TileOpacity));
                }
            }
        }

        private bool _isActive;
        public bool IsActive
        {
            get => _isActive;
            set => SetProperty(ref _isActive, value);
        }

        public double TileOpacity => _isIncluded ? 1.0 : 0.35;

        private Avalonia.Media.IImage? _thumbnail;
        public Avalonia.Media.IImage? Thumbnail
        {
            get => _thumbnail;
            private set
            {
                if (SetProperty(ref _thumbnail, value))
                {
                    OnPropertyChanged(nameof(HasThumbnail));
                }
            }
        }

        public bool HasThumbnail => _thumbnail != null;

        public ICommand ClickCommand { get; }

        public PaletteTileViewModel(uint index, string name, float weight, bool showWeight, bool isIncluded, bool isActive, Action<PaletteTileViewModel> onClick)
        {
            Index = index;
            Name = name;
            WeightText = showWeight ? $"×{weight:0.##}" : string.Empty;
            _isIncluded = isIncluded;
            _isActive = isActive;
            _onClick = onClick;

            ClickCommand = new RelayCommand(() => _onClick(this));
        }

        internal void RequestThumbnail()
        {
            ThumbnailService? thumbnailService = ThumbnailService.Instance;

            if (thumbnailService == null)
            {
                return;
            }

            Action<Avalonia.Media.IImage>? callback = null;
            callback = image =>
            {
                if (_thumbnailCallback == callback)
                {
                    Thumbnail = image;
                }
            };

            _thumbnailCallback = callback;

            thumbnailService.Request(AssetBucket.Prefabs.Value, new Name(Name), callback);
        }

        internal void ReleaseThumbnail()
        {
            if (_thumbnailCallback != null)
            {
                ThumbnailService.Instance?.Unsubscribe(AssetBucket.Prefabs.Value, new Name(Name), _thumbnailCallback);
                _thumbnailCallback = null;
            }
        }
    }

    public class InstancePainterPanelViewModel : SurfacePainterPanelViewModel
    {
        private const int MaxRecentPrefabs = 8;

        private static readonly List<string> s_recentPrefabs = new List<string>();

        private readonly EditorInstancePainterState _instancePainterState;

        private DelegateHandler? _onAssetsChangedHandler;

        public override bool HasGroundFitting => true;
        public override bool HasPalette => true;

        public ObservableCollection<PaintSourceItem> Sources { get; } = new ObservableCollection<PaintSourceItem>();
        public ObservableCollection<PaletteTileViewModel> Tiles { get; } = new ObservableCollection<PaletteTileViewModel>();

        private bool _applyingSource;

        private PaintSourceItem? _selectedSource;
        public PaintSourceItem? SelectedSource
        {
            get => _selectedSource;
            set
            {
                if (!SetProperty(ref _selectedSource, value) || _applyingSource || value == null)
                {
                    return;
                }

                PaintWith(value.Name, value.IsSet);
            }
        }

        private bool _isPaintingSet;

        public bool HasTiles => Tiles.Count > 0;

        private double _footprintRadius;
        public double FootprintRadius
        {
            get => _footprintRadius;
            set
            {
                if (SetProperty(ref _footprintRadius, value))
                {
                    _instancePainterState.SetFootprintRadius((float)value);
                }
            }
        }

        private double _sinkDepth;
        public double SinkDepth
        {
            get => _sinkDepth;
            set
            {
                if (SetProperty(ref _sinkDepth, value))
                {
                    _instancePainterState.SetSinkDepth((float)value);
                }
            }
        }

        private bool _onlyOnGround;
        public bool OnlyOnGround
        {
            get => _onlyOnGround;
            set
            {
                if (SetProperty(ref _onlyOnGround, value))
                {
                    _instancePainterState.SetOnlyOnGround(value);
                }
            }
        }

        private double _scaleMin;
        public double ScaleMin
        {
            get => _scaleMin;
            set
            {
                if (SetProperty(ref _scaleMin, value))
                {
                    _instancePainterState.SetScaleMin((float)value);
                }
            }
        }

        private double _scaleMax;
        public double ScaleMax
        {
            get => _scaleMax;
            set
            {
                if (SetProperty(ref _scaleMax, value))
                {
                    _instancePainterState.SetScaleMax((float)value);
                }
            }
        }

        public InstancePainterPanelViewModel(EditorSubsystem? editorSubsystem, EditorInstancePainterState? painterState, Action? onClosed)
            : base("Instance Painter", "Prefab", "ActivePrefab", AssetBucket.Prefabs, editorSubsystem, painterState, onClosed)
        {
            _instancePainterState = painterState!;

            _footprintRadius = painterState!.FootprintRadius;
            _sinkDepth = painterState.SinkDepth;
            _onlyOnGround = painterState.OnlyOnGround;
            _scaleMin = painterState.ScaleMin;
            _scaleMax = painterState.ScaleMax;

            _onAssetsChangedHandler = editorSubsystem!.GetOnAssetsChangedDelegate().Bind((uint bucketIndex) =>
            {
                if (bucketIndex == AssetBucket.Prefabs.Value || bucketIndex == AssetBucket.Misc.Value)
                {
                    Dispatcher.UIThread.Post(RefreshSources);
                    RefreshPalette();
                }
            });

            RefreshSources();
            RefreshPalette();

            Action? baseOnClosed = OnClosed;

            OnClosed = () =>
            {
                _onAssetsChangedHandler?.Remove();
                _onAssetsChangedHandler?.Dispose();
                _onAssetsChangedHandler = null;

                foreach (PaletteTileViewModel tile in Tiles)
                {
                    tile.ReleaseThumbnail();
                }

                baseOnClosed?.Invoke();
            };
        }

        private void PaintWith(string name, bool isSet)
        {
            if (!isSet)
            {
                s_recentPrefabs.Remove(name);
                s_recentPrefabs.Insert(0, name);

                if (s_recentPrefabs.Count > MaxRecentPrefabs)
                {
                    s_recentPrefabs.RemoveAt(s_recentPrefabs.Count - 1);
                }
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (isSet)
                {
                    _instancePainterState.SetActivePrefabSetByName(new Name(name));
                }
                else
                {
                    _instancePainterState.SetActivePrefabByName(new Name(name));
                }

                RefreshPalette();
            });
        }

        private void RefreshSources()
        {
            AssetRegistry registry = AssetManager.Instance.AssetRegistry;

            List<PaintSourceItem> sources = registry.GetBucketAssetDescs(AssetBucket.Misc.Value)
                .Select(desc => new PaintSourceItem(desc.Name.ToString(), isSet: true))
                .OrderBy(item => item.Name, StringComparer.OrdinalIgnoreCase)
                .Concat(registry.GetBucketAssetDescs(AssetBucket.Prefabs.Value)
                    .Select(desc => new PaintSourceItem(desc.Name.ToString(), isSet: false))
                    .OrderBy(item => item.Name, StringComparer.OrdinalIgnoreCase))
                .ToList();

            string? selectedName = _selectedSource?.Name;
            bool selectedIsSet = _selectedSource?.IsSet ?? false;

            _applyingSource = true;

            Sources.Clear();

            foreach (PaintSourceItem source in sources)
            {
                Sources.Add(source);
            }

            SelectedSource = Sources.FirstOrDefault(source => source.Name == selectedName && source.IsSet == selectedIsSet);

            _applyingSource = false;
        }

        private void RefreshPalette()
        {
            if (!EngineManager.IsOnSimThread)
            {
                _ = EngineManager.PostToSimThread(RefreshPalette);

                return;
            }

            string? setName = _instancePainterState.ActivePrefabSet?.Name.ToString();
            string? prefabName = _instancePainterState.ActivePrefab?.Name.ToString();

            uint count = _instancePainterState.GetPaletteCount();

            var entries = Enumerable.Range(0, (int)count)
                .Select(i => (
                    Index: (uint)i,
                    Name: _instancePainterState.GetPaletteName((uint)i).ToString(),
                    Weight: _instancePainterState.GetPaletteWeight((uint)i),
                    Included: _instancePainterState.IsPaletteEntryIncluded((uint)i)))
                .ToList();

            Dispatcher.UIThread.Post(() => ApplyPalette(setName, prefabName, entries));
        }

        private void ApplyPalette(string? setName, string? prefabName, List<(uint Index, string Name, float Weight, bool Included)> entries)
        {
            foreach (PaletteTileViewModel tile in Tiles)
            {
                tile.ReleaseThumbnail();
            }

            Tiles.Clear();

            _isPaintingSet = setName != null;

            if (_isPaintingSet)
            {
                bool showWeights = entries.Any(entry => Math.Abs(entry.Weight - entries[0].Weight) > 1e-4f);

                foreach (var entry in entries)
                {
                    Tiles.Add(new PaletteTileViewModel(entry.Index, entry.Name, entry.Weight, showWeights, entry.Included, isActive: false, OnTileClicked));
                }
            }
            else
            {
                if (prefabName != null && !s_recentPrefabs.Contains(prefabName))
                {
                    s_recentPrefabs.Insert(0, prefabName);
                }

                HashSet<string> existing = AssetManager.Instance.AssetRegistry.GetBucketAssetDescs(AssetBucket.Prefabs.Value)
                    .Select(desc => desc.Name.ToString())
                    .ToHashSet(StringComparer.Ordinal);

                s_recentPrefabs.RemoveAll(name => !existing.Contains(name));

                uint index = 0;

                foreach (string name in s_recentPrefabs)
                {
                    Tiles.Add(new PaletteTileViewModel(index++, name, 1.0f, showWeight: false, isIncluded: true, isActive: name == prefabName, OnTileClicked));
                }
            }

            foreach (PaletteTileViewModel tile in Tiles)
            {
                tile.RequestThumbnail();
            }

            _applyingSource = true;
            SelectedSource = Sources.FirstOrDefault(source => _isPaintingSet ? source.IsSet && source.Name == setName : !source.IsSet && source.Name == prefabName);
            _applyingSource = false;

            OnPropertyChanged(nameof(HasTiles));
        }

        private void OnTileClicked(PaletteTileViewModel tile)
        {
            if (_isPaintingSet)
            {
                bool include = !tile.IsIncluded;

                tile.IsIncluded = include;

                _ = EngineManager.PostToSimThread(() =>
                {
                    _instancePainterState.SetPaletteEntryIncluded(tile.Index, include);

                    RefreshPalette();
                });

                return;
            }

            PaintWith(tile.Name, isSet: false);
        }
    }
}
