using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorViewModel : ViewModelBase
    {
        public InspectorSectionViewModel ComponentsSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel TagsSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel LayersSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel ScriptSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel ActionsSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel ScenePropertiesSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel SwatchOverridesSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel PropertiesSection { get; } = new() { IsExpanded = true };

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();
        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();
        public ObservableCollection<InspectorComponentViewModelBase> Components { get; } = new ObservableCollection<InspectorComponentViewModelBase>();
        public ObservableCollection<AddComponentOptionViewModel> AddableComponents { get; } = new ObservableCollection<AddComponentOptionViewModel>();

        public ICommand AddComponentCommand { get; }
        public ICommand RemoveComponentCommand { get; }

        public ObservableCollection<SwatchCopySourceOptionViewModel> CopySwatchSources { get; } = new ObservableCollection<SwatchCopySourceOptionViewModel>();

        private SwatchCopySourceOptionViewModel? _selectedCopySwatchSource;
        public SwatchCopySourceOptionViewModel? SelectedCopySwatchSource
        {
            get => _selectedCopySwatchSource;
            set
            {
                if (SetProperty(ref _selectedCopySwatchSource, value) && ApplyCopyFromSwatchCommand is AsyncRelayCommand relayCommand)
                {
                    relayCommand.RaiseCanExecuteChanged();
                }
            }
        }

        private bool _hasCopySwatchSources;
        public bool HasCopySwatchSources
        {
            get => _hasCopySwatchSources;
            private set => SetProperty(ref _hasCopySwatchSources, value);
        }

        private bool _hasActiveSwatchOverrides;
        public bool HasActiveSwatchOverrides
        {
            get => _hasActiveSwatchOverrides;
            private set
            {
                if (SetProperty(ref _hasActiveSwatchOverrides, value) && ResetSwatchOverridesCommand is AsyncRelayCommand relayCommand)
                {
                    relayCommand.RaiseCanExecuteChanged();
                }
            }
        }

        /// <summary>Display-only view over <see cref="Properties"/>: the rows the active swatch overrides.</summary>
        public ObservableCollection<InspectorPropertyViewModelBase> OverriddenProperties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        private int _overriddenPropertyCount;
        public int OverriddenPropertyCount
        {
            get => _overriddenPropertyCount;
            private set
            {
                if (SetProperty(ref _overriddenPropertyCount, value))
                {
                    OnPropertyChanged(nameof(HasOverriddenProperties));
                    OnPropertyChanged(nameof(ShowNoOverridesHint));
                    OnPropertyChanged(nameof(OverriddenPropertiesHeader));
                }
            }
        }

        public bool HasOverriddenProperties => _overriddenPropertyCount > 0;

        public bool ShowNoOverridesHint => CanUseSwatchOverrides && !HasOverriddenProperties;

        public string OverriddenPropertiesHeader => _overriddenPropertyCount == 1
            ? $"1 PROPERTY OVERRIDDEN IN {ActiveSwatchLabel.ToUpperInvariant()}"
            : $"{_overriddenPropertyCount} PROPERTIES OVERRIDDEN IN {ActiveSwatchLabel.ToUpperInvariant()}";

        public ICommand ApplyCopyFromSwatchCommand { get; }
        public ICommand ResetSwatchOverridesCommand { get; }

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        private bool _hasComponents;
        public bool HasComponents
        {
            get => _hasComponents;
            private set => SetProperty(ref _hasComponents, value);
        }

        private bool _hasAddableComponents;
        public bool HasAddableComponents
        {
            get => _hasAddableComponents;
            private set => SetProperty(ref _hasAddableComponents, value);
        }

        private bool _isEntity;
        /// <summary>True when the selected node is an entity - with a multi-selection, when every selected node is.</summary>
        public bool IsEntity
        {
            get => _isEntity;
            private set
            {
                if (SetProperty(ref _isEntity, value))
                {
                    OnPropertyChanged(nameof(IsSingleEntity));
                }
            }
        }

        private bool _isRootNode;
        public bool IsRootNode
        {
            get => _isRootNode;
            private set => SetProperty(ref _isRootNode, value);
        }

        private bool _hasSceneProperties;
        public bool HasSceneProperties
        {
            get => _hasSceneProperties;
            private set => SetProperty(ref _hasSceneProperties, value);
        }

        public ObservableCollection<InspectorPropertyViewModelBase> SceneProperties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        private AttachedScriptViewModel? _attachedScript;
        public AttachedScriptViewModel? AttachedScript
        {
            get => _attachedScript;
            private set => SetProperty(ref _attachedScript, value);
        }

        private bool _hasAttachedScript;
        public bool HasAttachedScript
        {
            get => _hasAttachedScript;
            private set => SetProperty(ref _hasAttachedScript, value);
        }

        private EntityTagsViewModel? _entityTags;
        public EntityTagsViewModel? EntityTags
        {
            get => _entityTags;
            private set => SetProperty(ref _entityTags, value);
        }

        private EntityLayersViewModel? _entityLayers;
        public EntityLayersViewModel? EntityLayers
        {
            get => _entityLayers;
            private set => SetProperty(ref _entityLayers, value);
        }

        public bool IsDefaultSwatch
        {
            get => (_activeSwatchDisplay ?? string.Empty) == string.Empty
                || (_activeSwatchDisplay ?? string.Empty) == "Default";
        }

        public bool CanUseSwatchOverrides => !IsDefaultSwatch;

        private void ApplyActiveSwatchToEditContext()
        {
            SwatchOverrideEditContext.ActiveSwatchName = IsDefaultSwatch ? null : ActiveSwatchDisplay;

            if (IsDefaultSwatch)
            {
                SwatchOverrideMode = false;
            }
        }

        private bool _swatchOverrideMode;
        public bool SwatchOverrideMode
        {
            get => _swatchOverrideMode;
            set
            {
                if (SetProperty(ref _swatchOverrideMode, value))
                {
                    SwatchOverrideEditContext.OverrideModeActive = value;

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        EngineManager.EditorGame?.EditorSubsystem?.SetSwatchOverrideMode(value);
                    });
                    _ = RefreshCopySwatchSourcesAsync();
                }
            }
        }

        /// <summary>Active swatch name for display, falling back to "Default" before the World reports one.</summary>
        public string ActiveSwatchLabel => string.IsNullOrEmpty(_activeSwatchDisplay) ? "Default" : _activeSwatchDisplay!;

        private string? _activeSwatchDisplay;
        public string? ActiveSwatchDisplay
        {
            get => _activeSwatchDisplay;
            private set
            {
                if (SetProperty(ref _activeSwatchDisplay, value))
                {
                    OnPropertyChanged(nameof(IsDefaultSwatch));
                    OnPropertyChanged(nameof(CanUseSwatchOverrides));
                    OnPropertyChanged(nameof(ShowNoOverridesHint));
                    OnPropertyChanged(nameof(ActiveSwatchLabel));
                    OnPropertyChanged(nameof(OverriddenPropertiesHeader));
                }
            }
        }

        private Node? _selectedNode;
        /// <summary>The node the inspector is built from; with a multi-selection, the primary (focused) one.</summary>
        public Node? SelectedNode
        {
            get => _selectedNode;
            private set => SetProperty(ref _selectedNode, value);
        }

        // Every node shown, SelectedNode first.
        private IReadOnlyList<Node> _selectedNodes = Array.Empty<Node>();
        public IReadOnlyList<Node> SelectedNodes => _selectedNodes;

        private bool _isMultiSelection;
        /// <summary>True when several nodes are selected: only what they have in common is shown, and edits apply to all of them.</summary>
        public bool IsMultiSelection
        {
            get => _isMultiSelection;
            private set
            {
                if (SetProperty(ref _isMultiSelection, value))
                {
                    OnPropertyChanged(nameof(IsSingleEntity));
                }
            }
        }

        /// <summary>For sections that only make sense for one entity (tags, layers, swatch overrides).</summary>
        public bool IsSingleEntity => IsEntity && !IsMultiSelection;

        public string SelectionSummary => $"{_selectedNodes.Count} objects selected";

        private readonly List<DelegateHandler> _transformUpdatedHandlers = new List<DelegateHandler>();

        // Bumped on every rebuild so results read for a previous selection are dropped.
        private int _refreshGeneration;

        private Scene? _currentScene;
        public Scene? CurrentScene
        {
            get => _currentScene;
            private set => SetProperty(ref _currentScene, value);
        }

        public InspectorViewModel()
        {
            AddComponentCommand = new AsyncRelayCommand(AddComponentAsync, CanAddComponent);
            RemoveComponentCommand = new RelayCommand<object>(RemoveComponent, CanRemoveComponent);
            ApplyCopyFromSwatchCommand = new AsyncRelayCommand(_ => ApplyCopyFromSwatchAsync(), _ => SelectedCopySwatchSource != null);
            ResetSwatchOverridesCommand = new AsyncRelayCommand(_ => ResetSwatchOverridesAsync(), _ => HasActiveSwatchOverrides);
        }

        ~InspectorViewModel()
        {
            UnbindTransformUpdated();
        }

        public void SetSelectedNode(Node? node, Scene? scene = null, bool isRootNode = false)
        {
            SetSelection(node != null ? new[] { node } : Array.Empty<Node>(), node, scene, isRootNode);
        }

        /// <summary>
        /// Shows several nodes at once: the properties and components they all have, with a value
        /// shown only where they agree, and edits applied to every one of them.
        /// </summary>
        public void SetSelectedNodes(IReadOnlyList<Node> nodes, Node? primaryNode, Scene? scene = null)
        {
            SetSelection(nodes, primaryNode, scene, isRootNode: false);
        }

        /// <summary>True when the inspector already shows exactly these nodes (in any order).</summary>
        public bool IsShowingSelection(IReadOnlyList<Node> nodes)
        {
            if (_selectedNodes.Count != nodes.Count)
            {
                return false;
            }

            HashSet<IntPtr> shown = _selectedNodes.Select(n => n.NativeAddress).ToHashSet();

            return nodes.All(n => shown.Contains(n.NativeAddress));
        }

        private void SetSelection(IReadOnlyList<Node> nodes, Node? primaryNode, Scene? scene, bool isRootNode)
        {
            Dispatcher.UIThread.VerifyAccess();

            UnbindTransformUpdated();

            List<Node> ordered = nodes
                .Where(n => n != null && n.IsValid)
                .GroupBy(n => n.NativeAddress)
                .Select(g => g.First())
                .ToList();

            primaryNode ??= ordered.FirstOrDefault();

            if (primaryNode != null)
            {
                ordered.RemoveAll(n => n.NativeAddress == primaryNode.NativeAddress);
                ordered.Insert(0, primaryNode);
            }

            _selectedNodes = ordered;
            OnPropertyChanged(nameof(SelectedNodes));
            OnPropertyChanged(nameof(SelectionSummary));

            SelectedNode = primaryNode;
            CurrentScene = scene;
            IsRootNode = isRootNode && ordered.Count <= 1;
            IsMultiSelection = ordered.Count > 1;

            foreach (Node node in ordered.Where(n => n.IsValid))
            {
                _transformUpdatedHandlers.Add(node.GetTransformUpdatedDelegate().Bind((Node updatedNode) =>
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        RefreshTransformProperties();
                    });
                }));
            }

            RefreshProperties();
        }

        private void UnbindTransformUpdated()
        {
            foreach (DelegateHandler handler in _transformUpdatedHandlers)
            {
                handler.Remove();
            }

            _transformUpdatedHandlers.Clear();
        }

        private void RefreshProperties()
        {
            Dispatcher.UIThread.VerifyAccess();

            int refreshGeneration = ++_refreshGeneration;

            Properties.Clear();
            Actions.Clear();
            Components.Clear();
            AddableComponents.Clear();
            SceneProperties.Clear();
            CopySwatchSources.Clear();

            AttachedScript = null;
            HasAttachedScript = false;
            EntityTags = null;
            EntityLayers = null;

            SelectedCopySwatchSource = null;
            HasCopySwatchSources = false;
            HasActiveSwatchOverrides = false;

            OverriddenProperties.Clear();
            OverriddenPropertyCount = 0;

            SwatchOverrideEditContext.Reset();

            HasActions = false;
            HasComponents = false;
            HasAddableComponents = false;
            HasSceneProperties = false;

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            Node primaryNode = SelectedNode;
            List<Node> peerNodes = _selectedNodes.Skip(1).Where(n => n.IsValid).ToList();
            bool isMultiSelection = peerNodes.Count > 0;

            // If this is the root node, show scene properties
            if (IsRootNode && CurrentScene != null && CurrentScene.IsValid)
            {
                RefreshSceneProperties();
            }

            Class nodeClass = primaryNode.Class;

            // With a multi-selection, only properties every selected node has are shown.
            // sort by editororder attribute (if present), then by name
            List<(Property Property, List<Property> PeerProperties)> properties = nodeClass.Properties
                .Where(p =>
                {
                    ClassAttribute? attrEditCondition = p.GetAttribute("editcondition");

                    return EvaluateEditCondition(primaryNode, nodeClass, attrEditCondition, p.Name.ToString());
                })
                .Select(p => (Property: p, PeerProperties: FindPeerProperties(peerNodes, p)))
                .Where(entry => entry.PeerProperties != null)
                .Select(entry => (entry.Property, entry.PeerProperties!))
                .OrderBy(entry =>
                {
                    ClassAttribute? attrEditOrder = entry.Property.GetAttribute("editororder");

                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }

                    return int.MaxValue;
                })
                .ThenBy(entry => entry.Property.Name.ToString())
                .ToList();

            bool hasAddedMobility = false;

            var addMobility = () =>
            {
                try
                {
                    Property flagsProperty = Class.GetClass<Node>().GetProperty("NodeFlags") ?? throw new Exception("Failed to get NodeFlags property");

                    var mobilityVm = new MobilityPropertyViewModel(primaryNode, flagsProperty);
                    mobilityVm.AttachPeers(peerNodes.Select(peer => PropertyTarget.ForObject(peer, flagsProperty)).ToList());
                    mobilityVm.RefreshValue();

                    Properties.Add(mobilityVm);

                    hasAddedMobility = true;
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create mobility selector: {ex.Message}");
                }
            };

            foreach ((Property property, List<Property> peerProperties) in properties)
            {
                try
                {
                    if (property.Name == "Components")
                    {
                        continue; // skip Components property -- they're handled separately
                    }

                    if (property.Name == "Tags")
                    {
                        continue; // skip Entity Tags property -- they're handled separately
                    }

                    // skip non-editor properties
                    ClassAttribute? attrEditor = property.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        continue;
                    }

                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    InspectorPropertyViewModelBase vm = InspectorViewModelFactory.Create(
                        primaryNode, property, isReadOnly, 0, null, CreateNodePostWrite(primaryNode, property), OnPropertyValueChanged,
                        initialize: !isMultiSelection);

                    if (isMultiSelection)
                    {
                        List<PropertyTarget> peers = peerNodes
                            .Select((peer, i) => PropertyTarget.ForObject(peer, peerProperties[i], CreateNodePostWrite(peer, peerProperties[i])))
                            .ToList();

                        if (!vm.AttachPeers(peers))
                        {
                            continue;
                        }

                        vm.RefreshValue();
                    }

                    Properties.Add(vm);

                    if (vm is FlagsPropertyViewModel flagsVm)
                    {
                        flagsVm.ValueCommitted += RefreshActions;

                        // Insert Mobility selector right after the Flags property
                        if (!hasAddedMobility)
                        {
                            addMobility();
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for property '{property.Name}': {ex.Message}");
                }
            }

            if (!hasAddedMobility)
            {
                addMobility();
            }

            RefreshActions();

            // collect components - with a multi-selection, the ones every selected entity has
            if (primaryNode is Entity entity && peerNodes.All(n => n is Entity))
            {
                IsEntity = true;

                List<Entity> peerEntities = peerNodes.Cast<Entity>().ToList();

                if (!isMultiSelection)
                {
                    AttachedScript = new AttachedScriptViewModel(entity);
                    HasAttachedScript = true;
                    EntityTags = new EntityTagsViewModel(entity);
                    EntityLayers = new EntityLayersViewModel(entity);

                    SwatchOverrideEditContext.CurrentEntity = entity;
                }

                // Also puts the active swatch back in the edit context, which multi-selection edits are routed by too.
                _ = RefreshActiveSwatchInfoAsync();

                if (!isMultiSelection)
                {
                    _ = RefreshOverrideSignifiersAsync();
                }

                _ = EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;
                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to get EntityManager for entity '{entity.Name}'");

                        return;
                    }

                    List<TypeId> componentTypeIds = mgr.GetComponentTypeIds(entity).ToList();

                    foreach (Entity peerEntity in peerEntities)
                    {
                        EntityManager? peerMgr = peerEntity.EntityManager;
                        HashSet<TypeId> peerTypeIds = peerMgr != null ? peerMgr.GetComponentTypeIds(peerEntity).ToHashSet() : new HashSet<TypeId>();

                        componentTypeIds.RemoveAll(typeId => !peerTypeIds.Contains(typeId));
                    }

                    Dispatcher.UIThread.Post(() =>
                    {
                        // The selection changed while the component list was being read.
                        if (refreshGeneration != _refreshGeneration)
                        {
                            return;
                        }

                        Components.Clear();

                        foreach (TypeId typeId in componentTypeIds)
                        {
                            InspectorComponentViewModelBase? componentVm = null;

                            ComponentTypeDescriptor? descriptor = RegisteredComponents
                                .FirstOrDefault(d => d.TypeId == typeId);

                            if (descriptor != null)
                                componentVm = descriptor.CreateViewModel(entity);
                            else
                                Logger.Log(LogLevel.Debug, $"Inspector has no view model for component type '{typeId}'");

                            if (componentVm != null && componentVm.IsEditorVisible)
                            {
                                componentVm.PeerEntities = peerEntities;

                                Components.Add(componentVm);
                                componentVm.PopulateProperties();
                            }
                        }

                        HasComponents = Components.Count > 0;

                        // Adding a component is offered for a single entity only.
                        if (!isMultiSelection)
                        {
                            UpdateAddableComponents(componentTypeIds);
                        }
                    });
                });
            }
            else
            {
                IsEntity = false;
                AttachedScript = null;
                HasAttachedScript = false;
                AddableComponents.Clear();
                HasAddableComponents = false;
            }
        }

        // The matching property of each peer node, or null when any of them lacks it (or its editcondition fails).
        private List<Property>? FindPeerProperties(List<Node> peerNodes, Property property)
        {
            List<Property> peerProperties = new List<Property>(peerNodes.Count);

            foreach (Node peer in peerNodes)
            {
                Class peerClass = peer.Class;
                Property? peerProperty = peerClass.GetProperty(property.Name);

                if (peerProperty == null || peerProperty.Value.TypeInfo.Name != property.TypeInfo.Name)
                {
                    return null;
                }

                if (!EvaluateEditCondition(peer, peerClass, peerProperty.Value.GetAttribute("editcondition"), property.Name.ToString()))
                {
                    return null;
                }

                peerProperties.Add(peerProperty.Value);
            }

            return peerProperties;
        }

        private static Action? CreateNodePostWrite(Node node, Property property)
        {
            if (property.Name == "LocalBounds" && node is Entity localBoundsEntity)
            {
                return () =>
                {
                    EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                        new Name("EditorCommandSyncPhysicsShapeToLocalBounds"),
                        localBoundsEntity.NativeAddress.ToString());
                };
            }

            return null;
        }

        private async void RefreshActions()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (SelectedNode == null || !SelectedNode.IsValid || IsMultiSelection)
                return;

            Node node = SelectedNode;
            int refreshGeneration = _refreshGeneration;

            List<InspectorActionViewModel> actionVms = await InspectorActionsHelper.GetActionsAsync(node, OnPropertyValueChanged);

            if (refreshGeneration != _refreshGeneration)
            {
                return;
            }

            Actions.Clear();

            foreach (InspectorActionViewModel actionVm in actionVms)
            {
                Actions.Add(actionVm);
            }

            HasActions = Actions.Count > 0;
        }

        /// <summary>
        /// Re-reads every property shown for the selected node. A setter can change more than the
        /// value it was given (clamped ranges, packed flag bits, side effects on other members), so
        /// after any write the whole object is read back instead of trusting what the UI sent.
        /// </summary>
        private void OnPropertyValueChanged()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                propertyVm.RefreshValue();
            }

            foreach (InspectorComponentViewModelBase componentVm in Components)
            {
                componentVm.RefreshProperties();
            }

            if (EntityTags != null)
            {
                _ = EntityTags.RefreshAsync();
            }

            if (EntityLayers != null)
            {
                _ = EntityLayers.RefreshAsync();
            }

            _ = RefreshOverrideSignifiersAsync();
            _ = RefreshActiveSwatchOverridesAsync();
        }

        /// <summary>
        /// Reads the World's active swatch name for the selected entity and updates the
        /// swatch-override edit context + panel hint text.
        /// </summary>
        public async Task RefreshActiveSwatchInfoAsync()
        {
            string activeSwatchName = string.Empty;

            await EngineManager.PostToSimThread(() =>
            {
                World? world = null;

                if (SelectedNode is Entity selectedEntity && selectedEntity.IsValid)
                {
                    world = selectedEntity.World;
                }

                if (world == null)
                {
                    EditorProject? project = EngineManager.CurrentProject;

                    if (project != null)
                    {
                        world = project.World;
                    }
                }

                if (world != null)
                {
                    activeSwatchName = world.GetActiveSwatchName().ToString();
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                ActiveSwatchDisplay = activeSwatchName.Length > 0 ? activeSwatchName : null;
                ApplyActiveSwatchToEditContext();
            });

            _ = RefreshCopySwatchSourcesAsync();
            _ = RefreshActiveSwatchOverridesAsync();
        }

        /// <summary>
        /// Refreshes per-row override signifiers ("SwatchA, SwatchB override this value") for the
        /// selected entity's entity-level property rows.
        /// </summary>
        public async Task RefreshOverrideSignifiersAsync()
        {
            // Override markers describe one entity's swatch overrides.
            if (IsMultiSelection || SelectedNode is not Entity entity || !entity.IsValid)
            {
                return;
            }

            // Entity-level rows only (component / delegate-backed rows are not overridable in v1);
            // Name is identity metadata and never overridable
            List<InspectorPropertyViewModelBase> rows = Properties
                .Where(p => p.IsEntityLevelRow && p.Property.Name != new Name("Name", weak: true))
                .ToList();

            List<string> swatchNames = new();
            List<bool> overriddenFlags = new();

            await EngineManager.PostToSimThread(() =>
            {
                Name[] sets = EntitySwatchOverrides.GetSetSwatchNames(entity);

                foreach (Name set in sets)
                {
                    swatchNames.Add(set.ToString());
                }

                foreach (InspectorPropertyViewModelBase row in rows)
                {
                    Name propertyName = row.Property.Name;

                    foreach (Name swatch in sets)
                    {
                        overriddenFlags.Add(EntitySwatchOverrides.IsPropertyOverridden(entity, swatch, propertyName));
                    }
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                if (SelectedNode is not Entity selectedEntity || selectedEntity.NativeAddress != entity.NativeAddress)
                {
                    return;
                }

                int flagIndex = 0;
                string? currentSwatchName = SwatchOverrideEditContext.ActiveSwatchName;

                OverriddenProperties.Clear();

                foreach (InspectorPropertyViewModelBase row in rows)
                {
                    List<string> overriddenSwatches = new();

                    foreach (string swatchName in swatchNames)
                    {
                        if (flagIndex < overriddenFlags.Count && overriddenFlags[flagIndex++])
                        {
                            overriddenSwatches.Add(swatchName);
                        }
                    }

                    row.SetOverrideInfo(overriddenSwatches, currentSwatchName);

                    if (row.IsOverriddenByCurrentSwatch)
                    {
                        OverriddenProperties.Add(row);
                    }
                }

                OverriddenPropertyCount = OverriddenProperties.Count;
            });
        }

        /// <summary>
        /// Called when the World's active swatch changes; refreshes property rows and override
        /// signifiers (overrides may have applied natively) and updates the edit context.
        /// </summary>
        public void OnWorldActiveSwatchChanged(string swatchName)
        {
            Dispatcher.UIThread.VerifyAccess();

            ActiveSwatchDisplay = string.IsNullOrEmpty(swatchName) ? null : swatchName;
            ApplyActiveSwatchToEditContext();

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            // Overrides for the new active swatch were just applied natively - re-read all rows
            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                propertyVm.RefreshValue();
            }

            _ = RefreshOverrideSignifiersAsync();
            _ = RefreshCopySwatchSourcesAsync();
            _ = RefreshActiveSwatchOverridesAsync();
        }

        /// <summary>
        /// Refreshes whether the selected entity has any property overrides in the World's
        /// active swatch; drives the "Reset Swatch Overrides" button enablement.
        /// </summary>
        private async Task RefreshActiveSwatchOverridesAsync()
        {
            Entity? entity = SelectedNode as Entity;
            string? swatchName = ActiveSwatchDisplay;

            bool hasOverrides = false;
            Entity? capturedEntity = null;

            if (entity != null && entity.IsValid && !string.IsNullOrEmpty(swatchName))
            {
                capturedEntity = entity;

                string capturedSwatch = swatchName;

                await EngineManager.PostToSimThread(() =>
                {
                    hasOverrides = EntitySwatchOverrides.HasValues(capturedEntity, new Name(capturedSwatch));
                });
            }

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                if (capturedEntity == null
                    || SelectedNode is not Entity currentEntity || !currentEntity.IsValid
                    || currentEntity.NativeAddress != capturedEntity.NativeAddress)
                {
                    return;
                }

                HasActiveSwatchOverrides = hasOverrides;
            });
        }

        /// <summary>
        /// Invokes the native <c>EditorCommandResetSwatchOverrides</c> command, removing all
        /// property overrides the entity has in the World's active swatch (restoring base
        /// values). Undoable.
        /// </summary>
        private async Task ResetSwatchOverridesAsync()
        {
            if (SelectedNode is not Entity entity || !entity.IsValid)
            {
                return;
            }

            if (string.IsNullOrEmpty(ActiveSwatchDisplay))
            {
                return;
            }

            Entity capturedEntity = entity;

            await EngineManager.PostToSimThread(() =>
            {
                EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                    new Name("EditorCommandResetSwatchOverrides"),
                    capturedEntity.NativeAddress.ToString());
            });

            await RefreshActiveSwatchOverridesAsync();
            
            OnPropertyValueChanged();
        }

        /// <summary>
        /// Rebuilds the "Copy Properties from Swatch" source options: the base state plus every
        /// World swatch except the current edit target (the active swatch when override mode is
        /// on, base otherwise). Copying a source onto itself would be a no-op.
        /// </summary>
        private async Task RefreshCopySwatchSourcesAsync()
        {
            Entity? entity = SelectedNode as Entity;

            if (entity == null || !entity.IsValid)
            {
                CopySwatchSources.Clear();
                SelectedCopySwatchSource = null;
                HasCopySwatchSources = false;

                return;
            }

            Entity capturedEntity = entity;
            List<string> swatchNames = new List<string>();

            await EngineManager.PostToSimThread(() =>
            {
                World? world = capturedEntity.World;

                if (world == null)
                {
                    EditorProject? project = EngineManager.CurrentProject;

                    if (project != null)
                    {
                        world = project.World;
                    }
                }

                if (world == null)
                {
                    return;
                }

                foreach (Name swatchName in world.GetSwatchNames())
                {
                    swatchNames.Add(swatchName.ToString());
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                if (SelectedNode is not Entity currentEntity || !currentEntity.IsValid
                    || currentEntity.NativeAddress != capturedEntity.NativeAddress)
                {
                    return;
                }

                string? targetSwatchName = SwatchOverrideMode ? ActiveSwatchDisplay : null;
                string? previousSelection = SelectedCopySwatchSource?.Name;

                CopySwatchSources.Clear();

                if (targetSwatchName != null)
                {
                    CopySwatchSources.Add(new SwatchCopySourceOptionViewModel("Base", isBase: true));
                }

                foreach (string swatchName in swatchNames)
                {
                    if (swatchName == targetSwatchName)
                    {
                        continue;
                    }

                    // Default holds the base values, so it is already covered by the "Base" option
                    if (swatchName == "Default")
                    {
                        continue;
                    }

                    CopySwatchSources.Add(new SwatchCopySourceOptionViewModel(swatchName, isBase: false));
                }

                HasCopySwatchSources = CopySwatchSources.Count > 0;

                SelectedCopySwatchSource = previousSelection != null
                    ? CopySwatchSources.FirstOrDefault(option => option.Name == previousSelection)
                    : null;
            });
        }

        /// <summary>
        /// Invokes the native <c>EditorCommandCopySwatchProperties</c> command: it computes the
        /// minimal diff to make the current edit target (the active swatch's override set when
        /// override mode is on, otherwise the entity's base values) match the selected source's
        /// effective values, applies it as a single undoable action, and prunes overrides that
        /// would end up redundant (e.g. copying from Base empties the target swatch's override
        /// set).
        /// </summary>
        private async Task ApplyCopyFromSwatchAsync()
        {
            SwatchCopySourceOptionViewModel? sourceOption = SelectedCopySwatchSource;

            if (sourceOption == null)
            {
                return;
            }

            if (SelectedNode is not Entity entity || !entity.IsValid)
            {
                return;
            }

            bool targetIsSwatch = !IsDefaultSwatch && !string.IsNullOrEmpty(ActiveSwatchDisplay);
            string targetDisplay = targetIsSwatch ? ActiveSwatchDisplay! : "Base";

            if (!targetIsSwatch && sourceOption.IsBase)
            {
                return; // base -> base is a no-op
            }

            if (targetIsSwatch && !sourceOption.IsBase && sourceOption.Name == targetDisplay)
            {
                return; // swatch -> itself is a no-op
            }

            await EngineManager.PostToSimThread(() =>
            {
                EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                    new Name("EditorCommandCopySwatchProperties"),
                    entity.NativeAddress.ToString(),
                    sourceOption.IsBase ? "1" : "0",
                    sourceOption.IsBase ? "-" : sourceOption.Name,
                    targetIsSwatch ? "0" : "1",
                    targetIsSwatch ? targetDisplay : "-");
            });

            // Re-read all rows + override signifiers (the command may have changed values)
            OnPropertyValueChanged();
        }

        private void OnScenePropertyValueChanged()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (CurrentScene == null || !CurrentScene.IsValid)
            {
                return;
            }

            foreach (InspectorPropertyViewModelBase propertyVm in SceneProperties)
            {
                propertyVm.RefreshValue();
            }
        }

        private void RefreshTransformProperties()
        {
            Dispatcher.UIThread.VerifyAccess();

            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                // Refresh transform-related properties
                if (propertyVm is TransformViewModel transformVm)
                {
                    transformVm.RefreshValue();
                }
            }
        }

        private void RefreshSceneProperties()
        {
            if (CurrentScene == null || !CurrentScene.IsValid)
            {
                return;
            }

            Class sceneClass = CurrentScene.Class;

            List<Property> sceneProps = sceneClass.Properties
                .Where(p =>
                {
                    if (p.Name.ToString() == "Root")
                    {
                        return false;
                    }

                    ClassAttribute? attrEditor = p.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        return false;
                    }

                    return true;
                })
                .OrderBy(p =>
                {
                    ClassAttribute? attrEditOrder = p.GetAttribute("editororder");
                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }
                    return int.MaxValue;
                })
                .ThenBy(p => p.Name.ToString())
                .ToList();

            foreach (Property property in sceneProps)
            {
                try
                {
                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    SceneProperties.Add(InspectorViewModelFactory.Create(
                        CurrentScene, property, isReadOnly, 0, null, null, OnScenePropertyValueChanged));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for scene property '{property.Name}': {ex.Message}");
                }
            }

            HasSceneProperties = SceneProperties.Count > 0;
        }

        private void UpdateAddableComponents(IEnumerable<TypeId> existingComponentTypes)
        {
            HashSet<TypeId> existingTypes = [.. existingComponentTypes];

            AddableComponents.Clear();

            foreach ((string label, TypeId typeId) in GetSupportedComponentTypes())
            {
                bool canAdd = !existingTypes.Contains(typeId);
                AddableComponents.Add(new AddComponentOptionViewModel(label, typeId, canAdd));
            }

            HasAddableComponents = AddableComponents.Any(option => option.IsEnabled);

            if (AddComponentCommand is AsyncRelayCommand relayCommand)
            {
                relayCommand.RaiseCanExecuteChanged();
            }
        }

        private static IEnumerable<(string Label, TypeId TypeId)> GetSupportedComponentTypes()
            => RegisteredComponents
                .Where(d => d.IsEditorEnabled)
                .Select(d => (d.Label, d.TypeId));

        private sealed record ComponentTypeDescriptor(
            string Label,
            Func<TypeId> GetTypeId,
            Func<Entity, InspectorComponentViewModelBase> CreateViewModel,
            Action<EntityManager, Entity> AddComponent)
        {
            // Evaluated on first access rather than at static init time
            public TypeId TypeId => GetTypeId();
            public bool IsEditorEnabled => Class.TryGetClass(TypeId)?.GetAttribute("editor")?.GetBool() ?? true;
        }

        private static ComponentTypeDescriptor? BuildDescriptor(Type componentType)
        {
            try
            {
                Class? cls = Class.TryGetClass(componentType);

                if (cls == null || !cls.Value.IsValid)
                    return null;

                Class componentClass = cls.Value;
                ClassAttribute? attrLabel = componentClass.GetAttribute("label");
                string label = attrLabel.HasValue ? attrLabel.Value.GetString() : componentClass.Name.ToString();

                Type vmType = typeof(InspectorComponentViewModel<>).MakeGenericType(componentType);

                return new ComponentTypeDescriptor(
                    label,
                    () => componentClass.TypeId,
                    entity => (InspectorComponentViewModelBase)Activator.CreateInstance(vmType, entity)!,
                    (mgr, entity) => mgr.AddDefaultComponent(entity, componentClass));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Inspector failed to build descriptor for '{componentType.Name}': {ex.Message}");
                return null;
            }
        }

        private static readonly object s_registeredComponentsLock = new();
        private static ComponentTypeDescriptor[]? s_registeredComponents;
        private static bool s_listeningForComponentTypeChanges;

        // Rebuilt after component types are registered, eg when a script assembly loads or hot reloads
        private static ComponentTypeDescriptor[] RegisteredComponents
        {
            get
            {
                lock (s_registeredComponentsLock)
                {
                    if (!s_listeningForComponentTypeChanges)
                    {
                        ComponentRegistry.ComponentTypesChanged += () =>
                        {
                            lock (s_registeredComponentsLock)
                            {
                                s_registeredComponents = null;
                            }
                        };

                        s_listeningForComponentTypeChanges = true;
                    }

                    return s_registeredComponents ??= BuildRegisteredComponents();
                }
            }
        }

        private static ComponentTypeDescriptor[] BuildRegisteredComponents()
            => AppDomain.CurrentDomain.GetAssemblies()
                .SelectMany(a =>
                {
                    try { return a.GetTypes(); }
                    catch { return Array.Empty<Type>(); }
                })
                .Where(t => t.IsValueType && t.GetInterfaces().Contains(typeof(IComponent)))
                // hot reloading a script assembly leaves the older copies of its types loaded; keep the newest
                .GroupBy(t => t.FullName)
                .Select(group => group.Last())
                .Select(BuildDescriptor)
                .OfType<ComponentTypeDescriptor>()
                .ToArray();

        private bool CanAddComponent(object? parameter)
        {
            return parameter is AddComponentOptionViewModel option && option.IsEnabled && SelectedNode is Entity && !IsMultiSelection;
        }

        private async Task AddComponentAsync(object? parameter)
        {
            if (parameter is not AddComponentOptionViewModel option)
            {
                return;
            }

            if (SelectedNode is not Entity entity || entity.EntityManager == null)
            {
                return;
            }

            try
            {
                await EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;

                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, "Inspector failed to get EntityManager while adding component");

                        return;
                    }

                    try
                    {
                        ComponentTypeDescriptor? descriptor = RegisteredComponents
                            .FirstOrDefault(d => d.TypeId == option.TypeId);

                        if (descriptor != null)
                            descriptor.AddComponent(mgr, entity);
                        else
                            Logger.Log(LogLevel.Warning, $"Inspector cannot add unsupported component type '{option.Label}'");
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to add component '{option.Label}': {ex.Message}");
                    }
                });
            }
            finally
            {
                Dispatcher.UIThread.Post(RefreshProperties);
            }
        }

        private bool CanRemoveComponent(object? parameter)
        {
            return parameter is InspectorComponentViewModelBase && SelectedNode is Entity && !IsMultiSelection;
        }

        private void RemoveComponent(object? parameter)
        {
            if (parameter is not InspectorComponentViewModelBase componentVm)
                return;

            if (SelectedNode is not Entity entity || entity.EntityManager == null)
                return;

            MessageBox.Info("Remove Component", $"Are you sure you want to remove the {componentVm.Label} component from {entity.Name}?")
                .Button("Remove", () => _ = RemoveComponentConfirmed(componentVm, entity))
                .Button("Cancel", () => { })
                .Show();
        }

        private async Task RemoveComponentConfirmed(InspectorComponentViewModelBase componentVm, Entity entity)
        {
            try
            {
                await EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;

                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, "Inspector failed to get EntityManager while removing component");
                        return;
                    }

                    try
                    {
                        mgr.RemoveComponent(entity, componentVm.TypeId);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to remove component '{componentVm.Label}': {ex.Message}");
                    }
                });
            }
            finally
            {
                Dispatcher.UIThread.Post(RefreshProperties);
            }
        }

        private bool EvaluateEditCondition(Node node, Class nodeClass, ClassAttribute? attrEditCondition, string memberName)
        {
            if (!node.IsValid)
            {
                return false;
            }

            if (attrEditCondition == null)
            {
                return true;
            }

            if (attrEditCondition.Value.IsString)
            {
                string methodName = attrEditCondition.Value.GetString();
                Method? conditionMethod = nodeClass.GetMethod(methodName);

                if (conditionMethod != null)
                {
                    using BoxedValue resultData = conditionMethod.Value.Invoke(node);
                    object? result = resultData.GetValue();

                    if (result is bool boolResult)
                    {
                        return boolResult;
                    }

                    Logger.Log(LogLevel.Warning, $"Inspector editcondition method '{methodName}' on member '{memberName}' did not return a bool");
                }
            }
            else if (attrEditCondition.Value.IsBool)
            {
                return attrEditCondition.Value.GetBool();
            }
            else
            {
                Logger.Log(LogLevel.Warning, $"Inspector editcondition attribute on member '{memberName}' is not a valid type");
            }

            return true; // continue if no condition or invalid condition
        }
    }

    /// <summary>Expand/collapse state + chevron glyph for one inspector section header.</summary>
    public class InspectorSectionViewModel : ViewModelBase
    {
        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set
            {
                if (SetProperty(ref _isExpanded, value))
                {
                    OnPropertyChanged(nameof(ChevronKind));
                }
            }
        }

        public string ChevronKind => _isExpanded ? "ChevronDown" : "ChevronRight";
    }

    public class AddComponentOptionViewModel : ViewModelBase
    {
        private bool _isEnabled;

        public AddComponentOptionViewModel(string label, TypeId typeId, bool isEnabled)
        {
            Label = label;
            TypeId = typeId;
            _isEnabled = isEnabled;
        }

        public string Label { get; }
        public TypeId TypeId { get; }

        public bool IsEnabled
        {
            get => _isEnabled;
            set => SetProperty(ref _isEnabled, value);
        }
    }

    public class SwatchCopySourceOptionViewModel : ViewModelBase
    {
        public SwatchCopySourceOptionViewModel(string name, bool isBase)
        {
            Name = name;
            IsBase = isBase;
        }

        public string Name { get; }

        /// <summary>True when this option refers to the entity's base state rather than a swatch.</summary>
        public bool IsBase { get; }
    }
}
