using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class WorldGridLayerViewModel : ViewModelBase
    {
        private readonly WorldSettingsPanelViewModel _owner;

        internal readonly WorldGrid WorldGrid;
        internal readonly WorldGridLayer Layer;

        public uint LayerTypeId { get; }
        public uint LayerId { get; }

        public string TypeName { get; }

        public bool IsTerrain { get; }

        public string IconKind => IsTerrain ? "AssetTerrain" : "Layers";

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        private string _displayName = string.Empty;
        public string DisplayName
        {
            get => _displayName;
            private set => SetProperty(ref _displayName, value);
        }

        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public WorldGridLayerViewModel(
            WorldSettingsPanelViewModel owner,
            WorldGrid worldGrid,
            WorldGridLayer layer,
            uint layerTypeId,
            uint layerId,
            string name,
            string typeName)
        {
            _owner = owner ?? throw new ArgumentNullException(nameof(owner));
            WorldGrid = worldGrid ?? throw new ArgumentNullException(nameof(worldGrid));
            Layer = layer ?? throw new ArgumentNullException(nameof(layer));

            LayerTypeId = layerTypeId;
            LayerId = layerId;

            TypeName = typeName ?? string.Empty;
            IsTerrain = TypeName.Equals("TerrainWorldGridLayer", StringComparison.Ordinal);

            _displayName = name ?? string.Empty;

            BuildPropertyViewModels();
            BuildActionViewModels();
        }

        private void BuildActionViewModels()
        {
            _ = LoadActionViewModelsAsync();
        }

        private async Task LoadActionViewModelsAsync()
        {
            Class? instanceClass = Class.TryGetClass(TypeName);

            List<InspectorActionViewModel> actionVms = await InspectorActionsHelper.GetActionsAsync(Layer, instanceClass ?? Layer.Class);

            foreach (InspectorActionViewModel action in actionVms)
            {
                Actions.Add(action);
            }

            HasActions = Actions.Count > 0;
        }

        private async void RefreshActions()
        {
            if (Actions.Count == 0)
            {
                return;
            }

            Class? instanceClass = Class.TryGetClass(TypeName);

            List<InspectorActionViewModel> refreshed = await InspectorActionsHelper.GetActionsAsync(Layer, instanceClass ?? Layer.Class);

            foreach (InspectorActionViewModel existing in Actions)
            {
                InspectorActionViewModel? updated = refreshed.FirstOrDefault(a => a.Label == existing.Label);

                if (updated != null)
                {
                    existing.SetEnabled(updated.IsEnabled);
                }
            }
        }

        private void BuildPropertyViewModels()
        {
            Name layerInfoPropertyName = new Name("LayerInfo");
            Name namePropertyName = new Name("Name");

            foreach (Property property in CollectPropertiesInherited(Layer.Class))
            {
                InspectorPropertyViewModelBase viewModel;

                try
                {
                    viewModel = InspectorViewModelFactory.Create(
                        Layer,
                        property,
                        isReadOnly: false,
                        postWriteCallback: property.Name == layerInfoPropertyName
                            ? CycleLayerInWorldGrid
                            : null,
                        valueChangedCallback: property.Name == namePropertyName
                            ? UpdateDisplayName
                            : RefreshActions);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Debug, $"WorldSettings: skipping property '{property.Name}' for layer '{DisplayName}': {ex.Message}");
                    continue;
                }

                Properties.Add(viewModel);
            }
        }

        /// <summary>
        /// Reflection properties are registered per-class, so derived classes don't
        /// include properties declared on their base classes (e.g. WorldGridLayer's
        /// "Name" / "LayerInfo"). Walk the inheritance chain base-most first and
        /// dedupe by name so derived declarations win.
        /// </summary>
        private static List<Property> CollectPropertiesInherited(Class cls)
        {
            var chain = new List<Class>();

            for (Class? current = cls; current.HasValue; current = current.Value.GetParent())
            {
                chain.Add(current.Value);
            }

            chain.Reverse();

            var propertiesByName = new Dictionary<string, Property>();

            foreach (Class current in chain)
            {
                foreach (Property property in current.Properties)
                {
                    propertiesByName[property.Name.ToString()] = property;
                }
            }

            return propertiesByName.Values.ToList();
        }

        private void CycleLayerInWorldGrid()
        {
            if (!WorldGrid.IsValid || !Layer.IsValid)
            {
                return;
            }

            WorldGrid.RemoveLayer(Layer);
            WorldGrid.AddLayer(Layer);
        }

        private void UpdateDisplayName()
        {
            if (!Layer.IsValid)
            {
                return;
            }

            try
            {
                DisplayName = Layer.GetName().ToString();
            }
            catch
            {
                // layer no longer valid / engine shutting down - keep the old name
            }
        }
    }
}
