using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using Avalonia.Media;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class NodeViewModel : ViewModelBase
    {
        private readonly Node _node;
        private readonly NodeViewModel? _parent;
        private readonly UUID _uuid;
        private int _actionsRefreshGeneration;

        public Node Node => _node;
        public NodeViewModel? Parent => _parent;

        public UUID UUID => _uuid;

        public bool IsRootNode => _parent == null;

        public bool IsInstance => this is InstanceNodeViewModel;
        public bool IsInstanceGroup => _node is InstanceGroup;

        public bool CanMoveToGrandparent => _parent?.Parent != null;

        public string MoveToGrandparentHeader => _parent?.Parent != null
            ? $"Move to {_parent.Parent.DisplayName}"
            : "Move to Parent";

        private string _name;
        public string Name
        {
            get => _name;
            private set
            {
                if (SetProperty(ref _name, value))
                {
                    // Update derived properties when name changes
                    OnPropertyChanged(nameof(DisplayName));
                    OnPropertyChanged(nameof(IsUnnamed));
                    OnPropertyChanged(nameof(NameFontStyle));
                }
            }
        }

        public string DisplayName => string.IsNullOrEmpty(_name) ? $"Unnamed {_node.GetType().Name}" : _name;
        public bool IsUnnamed => string.IsNullOrEmpty(_name);
        public FontStyle NameFontStyle => IsUnnamed
            ? FontStyle.Italic
            : FontStyle.Normal;

        public string ClassName => _node.Class.Name.ToString();

        public void RefreshNameFromEngine()
        {
            Name = _node.Name.ToString();
        }

        private bool _isEditingName;
        public bool IsEditingName
        {
            get => _isEditingName;
            set => SetProperty(ref _isEditingName, value);
        }

        private string _editingName = string.Empty;
        public string EditingName
        {
            get => _editingName;
            set => SetProperty(ref _editingName, value);
        }

        public void BeginRename()
        {
            EditingName = _name;
            IsEditingName = true;
        }

        public void CancelRename()
        {
            IsEditingName = false;
        }

        public string IconKind => _node switch
        {
            DirectionalLight    => "DirectionalLight",
            PointLight          => "PointLight",
            SpotLight           => "SpotLight",
            AreaRectLight       => "AreaLight",
            Camera              => "Camera",
            EnvProbe            => "EnvProbe",
            LightmapVolume      => "LightmapVolume",
            ParticleVolume      => "ParticleVolume",
            FogVolume           => "FogVolume",
            DecalProxy          => "Decal",
            Bone                => "AssetSkeleton",
            InstanceHandleNode  => "AssetPrefab",
            VolumeBase          => "Package",
            TextSprite          => "TextSprite",
            Sprite              => "Sprite",
            Entity              => "Entity",
            _                   => EditorPluginService.Instance.FindAssetIcon(_node.Class.Name.ToString()) ?? "Node",
        };

        private string _sourcePrefabName;

        public bool IsPrefabInstance => _sourcePrefabName.Length != 0;
        public string SourcePrefabName => _sourcePrefabName;
        public string SavePrefabHeader => $"Sync Prefab '{_sourcePrefabName}'";
        public string EditPrefabHeader => $"Edit Prefab '{_sourcePrefabName}'";
        public string PrefabInstanceTooltip => $"Prefab instance: {_sourcePrefabName}";

        public bool CanUsePrefabActions => !(MainWindowViewModel.Instance?.IsInEditMode ?? false);

        public void RefreshSourcePrefab()
        {
            Dispatcher.UIThread.VerifyAccess();

            _sourcePrefabName = EngineManager.EditorGame?.EditorSubsystem?.GetSourcePrefabName(_node) ?? string.Empty;

            OnPropertyChanged(nameof(IsPrefabInstance));
            OnPropertyChanged(nameof(SourcePrefabName));
            OnPropertyChanged(nameof(SavePrefabHeader));
            OnPropertyChanged(nameof(EditPrefabHeader));
            OnPropertyChanged(nameof(PrefabInstanceTooltip));
            OnPropertyChanged(nameof(CanUsePrefabActions));
        }

        private readonly List<NodeViewModel> _allChildren = new();

        public ObservableCollection<NodeViewModel> Children { get; } = new();

        public IReadOnlyList<NodeViewModel> AllChildren => _allChildren;

        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new();

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        public ObservableCollection<MoveToSceneItemViewModel> MoveToSceneTargets { get; } = new();

        private bool _hasMoveToSceneTargets;
        public bool HasMoveToSceneTargets
        {
            get => _hasMoveToSceneTargets;
            private set => SetProperty(ref _hasMoveToSceneTargets, value);
        }

        public ObservableCollection<ConvertToEntityTypeItemViewModel> ConvertToEntityTypeTargets { get; } = new();

        private bool _hasConvertToEntityTypeTargets;
        public bool HasConvertToEntityTypeTargets
        {
            get => _hasConvertToEntityTypeTargets;
            private set => SetProperty(ref _hasConvertToEntityTypeTargets, value);
        }

        private bool _canGenerateCollision;
        public bool CanGenerateCollision
        {
            get => _canGenerateCollision;
            private set => SetProperty(ref _canGenerateCollision, value);
        }

        private bool _canFitCollisionToMesh;
        public bool CanFitCollisionToMesh
        {
            get => _canFitCollisionToMesh;
            private set => SetProperty(ref _canFitCollisionToMesh, value);
        }

        public bool HasCollisionActions => _node is Entity;

        private ICommand? _setPhysicsShapeCommand;
        public ICommand SetPhysicsShapeCommand => _setPhysicsShapeCommand ??= new RelayCommand<string>(shapeClassName =>
        {
            if (string.IsNullOrEmpty(shapeClassName))
            {
                return;
            }

            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                new Name("EditorCommandSetPhysicsShape"),
                _uuid.ToString(),
                shapeClassName);
        });

        /// <summary>
        /// Reads whether the collision actions apply to this node. The component lookups behind them have
        /// to run on the sim thread, so the menu items start disabled and enable themselves a frame later.
        /// </summary>
        public void RefreshCollisionState()
        {
            Dispatcher.UIThread.VerifyAccess();

            Node node = _node;

            _ = EngineManager.PostToSimThread(() =>
            {
                EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

                if (editorSubsystem == null)
                {
                    return;
                }

                bool canGenerate = editorSubsystem.CanGenerateConvexCollision(node);
                bool canFit = editorSubsystem.CanFitPhysicsShapeToMesh(node);

                Dispatcher.UIThread.Post(() =>
                {
                    CanGenerateCollision = canGenerate;
                    CanFitCollisionToMesh = canFit;
                });
            });
        }

        public bool IsVolume => _node is VolumeBase;

        private bool _isOutsideSelection;

        /// <summary>
        /// Whether the context menu was opened on this node without it being selected (right-clicking a volume
        /// leaves the selection alone), in which case Copy and Delete act on this node rather than the selection.
        /// </summary>
        public bool IsOutsideSelection
        {
            get => _isOutsideSelection;
            private set => SetProperty(ref _isOutsideSelection, value);
        }

        private bool _canFitVolumeToSelection;
        public bool CanFitVolumeToSelection
        {
            get => _canFitVolumeToSelection;
            private set => SetProperty(ref _canFitVolumeToSelection, value);
        }

        private string _fitVolumeToSelectionHeader = "Fit to Selected Node";
        public string FitVolumeToSelectionHeader
        {
            get => _fitVolumeToSelectionHeader;
            private set => SetProperty(ref _fitVolumeToSelectionHeader, value);
        }

        public void RefreshSelectionContext(IReadOnlyCollection<NodeViewModel> selectedNodes)
        {
            Dispatcher.UIThread.VerifyAccess();

            IsOutsideSelection = !selectedNodes.Contains(this);

            if (!IsVolume)
            {
                return;
            }

            int fitTargetCount = selectedNodes.Count(selectedNode => !ReferenceEquals(selectedNode, this));
            FitVolumeToSelectionHeader = fitTargetCount > 1
                ? $"Fit to {fitTargetCount} Selected Nodes"
                : "Fit to Selected Node";

            // Bounds are read on the sim thread, so the item starts disabled and enables itself a frame later.
            CanFitVolumeToSelection = false;

            Node node = _node;

            _ = EngineManager.PostToSimThread(() =>
            {
                EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

                if (editorSubsystem == null)
                {
                    return;
                }

                bool canFit = editorSubsystem.CanFitVolumeToSelection(node);

                Dispatcher.UIThread.Post(() => CanFitVolumeToSelection = canFit);
            });
        }

        public async void RefreshActions()
        {
            Dispatcher.UIThread.VerifyAccess();

            int refreshGeneration = ++_actionsRefreshGeneration;

            List<InspectorActionViewModel> actionVms = await InspectorActionsHelper.GetActionsAsync(_node);

            if (refreshGeneration != _actionsRefreshGeneration)
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

        public void RefreshMoveToSceneTargets()
        {
            Dispatcher.UIThread.VerifyAccess();

            MoveToSceneTargets.Clear();

            MainWindowViewModel? mainWindowViewModel = MainWindowViewModel.Instance;

            if (mainWindowViewModel != null && !IsRootNode && !mainWindowViewModel.IsInEditMode)
            {
                Scene? currentScene = _node.IsValid ? _node.Scene : null;

                foreach (SceneViewModel sceneViewModel in mainWindowViewModel.Scenes)
                {
                    Scene? scene = sceneViewModel.Scene;

                    if (scene == null || !scene.IsValid || scene == currentScene)
                    {
                        continue;
                    }

                    MoveToSceneTargets.Add(new MoveToSceneItemViewModel(scene, _node));
                }
            }

            HasMoveToSceneTargets = MoveToSceneTargets.Count > 0;
        }

        public void RefreshConvertToEntityTypeTargets()
        {
            Dispatcher.UIThread.VerifyAccess();

            ConvertToEntityTypeTargets.Clear();

            MainWindowViewModel? mainWindowViewModel = MainWindowViewModel.Instance;

            if (mainWindowViewModel != null
                && !IsRootNode
                && !mainWindowViewModel.IsInEditMode
                && _node.IsValid && ClassName == "Node")
            {
                List<string> classNames = [];
                NameCallbackDelegate callback = (name, _) => classNames.Add(name);

                NativeBindings.Hyp_GetAllDerivedClassNames("Entity", callback, IntPtr.Zero);

                classNames.Sort(StringComparer.OrdinalIgnoreCase);

                foreach (string className in classNames)
                {
                    ConvertToEntityTypeTargets.Add(new ConvertToEntityTypeItemViewModel(className, _node));
                }
            }

            HasConvertToEntityTypeTargets = ConvertToEntityTypeTargets.Count > 0;
        }

        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set
            {
                if (SetProperty(ref _isExpanded, value) && value && _node is InstanceGroup && !_listAllInstances)
                {
                    _listAllInstances = true;
                    ScheduleInstanceRefresh();
                }
            }
        }

        private const uint MaxListedInstances = 100;

        private readonly List<NodeViewModel> _instanceRows = new();
        private DelegateHandler? _onInstanceListChanged;
        private int _isInstanceRefreshPending;

        private volatile bool _listAllInstances;

        private string _instanceCountText = string.Empty;
        public string InstanceCountText
        {
            get => _instanceCountText;
            private set => SetProperty(ref _instanceCountText, value);
        }

        public void ScheduleInstanceRefresh()
        {
            if (_node is not InstanceGroup instanceGroup || Interlocked.Exchange(ref _isInstanceRefreshPending, 1) != 0)
            {
                return;
            }

            WeakReference<NodeViewModel> weakThis = new(this);

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!weakThis.TryGetTarget(out NodeViewModel? target))
                {
                    return;
                }

                Interlocked.Exchange(ref target._isInstanceRefreshPending, 0);

                EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

                if (editorSubsystem == null || !instanceGroup.IsValid)
                {
                    return;
                }

                uint maxCount = target._listAllInstances ? MaxListedInstances : 1;

                List<Node> handles = editorSubsystem.GetInstanceHandles(instanceGroup, maxCount).Cast<Node>().ToList();
                uint numInstances = instanceGroup.NumInstances();

                Dispatcher.UIThread.Post(() => target.ApplyInstanceRows(handles, numInstances));
            });
        }

        private void ApplyInstanceRows(List<Node> handles, uint numInstances)
        {
            Dispatcher.UIThread.VerifyAccess();

            InstanceCountText = numInstances.ToString("N0") + " Instances";

            Dictionary<UUID, NodeViewModel> previousRows = [];
            foreach (NodeViewModel row in _instanceRows)
            {
                previousRows[row.UUID] = row;
            }

            List<NodeViewModel> rows = new List<NodeViewModel>(handles.Count);

            foreach (Node handle in handles)
            {
                if (handle == null || !handle.IsValid)
                {
                    continue;
                }

                if (!previousRows.Remove(handle.UUID, out NodeViewModel? row))
                {
                    row = new InstanceNodeViewModel(handle, this, _onChildrenChanged, _index);
                    _index?.Add(row);
                }

                rows.Add(row);
            }

            if (previousRows.Count == 0 && rows.SequenceEqual(_instanceRows))
            {
                return;
            }

            foreach (NodeViewModel staleRow in previousRows.Values)
            {
                _index?.Remove(staleRow);
                Children.Remove(staleRow);
            }

            _allChildren.RemoveAll(child => child.IsInstance);
            _allChildren.AddRange(rows);

            int firstRowIndex = Children.Count - (_instanceRows.Count - previousRows.Count);

            for (int i = 0; i < rows.Count; i++)
            {
                int targetIndex = firstRowIndex + i;

                if (targetIndex < Children.Count && ReferenceEquals(Children[targetIndex], rows[i]))
                {
                    continue;
                }

                int currentIndex = Children.IndexOf(rows[i]);

                if (currentIndex == -1)
                {
                    Children.Insert(targetIndex, rows[i]);
                }
                else
                {
                    Children.Move(currentIndex, targetIndex);
                }
            }

            _instanceRows.Clear();
            _instanceRows.AddRange(rows);

            _index?.NotifyInstanceRowsChanged();
        }

        private bool _isDropTarget;
        public bool IsDropTarget
        {
            get => _isDropTarget;
            set => SetProperty(ref _isDropTarget, value);
        }

        private bool _isFilteredOut;
        public bool IsFilteredOut
        {
            get => _isFilteredOut;
            private set => SetProperty(ref _isFilteredOut, value);
        }

        // @NOTE this remains for future extension,
        // but Nodes are no longer an AssetObject subclass, thus they have no Dirty
        public bool IsDirty => false;

        private DelegateHandler? _onChildAdded;
        private DelegateHandler? _onChildRemoved;

        private readonly Action? _onChildrenChanged;
        private readonly NodeViewModelIndex? _index;

        public NodeViewModel(Node node, NodeViewModel? parent = null, Action? onChildrenChanged = null, NodeViewModelIndex? index = null)
        {
            _node = node;
            _parent = parent;
            _onChildrenChanged = onChildrenChanged;
            _index = index;
            _uuid = node.UUID;
            _name = node.Name.ToString();

            // Root nodes are expanded by default
            _isExpanded = parent == null;

            if (IsInstance)
            {
                _sourcePrefabName = string.Empty;

                return;
            }

            _sourcePrefabName = EngineManager.EditorGame?.EditorSubsystem?.GetSourcePrefabName(node) ?? string.Empty;

            // Initialize existing children
            for (uint i = 0; i < node.NumChildren(); i++)
            {
                Node? child = node.GetChild(i);

                if (child != null)
                {
                    NodeViewModel childViewModel = new(child, this, onChildrenChanged, index);

                    _allChildren.Add(childViewModel);
                    Children.Add(childViewModel);
                }
            }

            WeakReference<NodeViewModel> weakThis = new WeakReference<NodeViewModel>(this);

            _onChildAdded = node.GetOnChildAddedDelegate().Bind((Node child, bool isDirect) =>
            {
                if (!isDirect)
                {
                    return;
                }

                NodeViewModel? target;
                if (!weakThis.TryGetTarget(out target))
                {
                    Logger.Log(LogLevel.Warning, "NodeViewModel target has been garbage collected before child added handler could be invoked.");
                    return;
                }

                // built inline on the thread attaching the child; its subtree may be mutated again before the UI gets to it
                NodeViewModel childViewModel = new(child, target, target!._onChildrenChanged, target!._index);

                Dispatcher.UIThread.Post(() =>
                {
                    int numInstanceRows = target!._instanceRows.Count;

                    target!._index?.Add(childViewModel);
                    target!._allChildren.Insert(target!._allChildren.Count - numInstanceRows, childViewModel);
                    target!.Children.Insert(target!.Children.Count - numInstanceRows, childViewModel);

                    target!._onChildrenChanged?.Invoke();
                });
            });

            _onChildRemoved = node.GetOnChildRemovedDelegate().Bind((Node child, bool isDirect) =>
            {
                if (!isDirect)
                {
                    return;
                }

                NodeViewModel? target;
                if (!weakThis.TryGetTarget(out target))
                {
                    Logger.Log(LogLevel.Warning, "NodeViewModel target has been garbage collected before child removed handler could be invoked.");
                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    for (int i = 0; i < target!._allChildren.Count; i++)
                    {
                        if (target!._allChildren[i].Node == child)
                        {
                            target!._index?.Remove(target!._allChildren[i]);
                            target!._allChildren.RemoveAt(i);
                            break;
                        }
                    }

                    for (int i = 0; i < target!.Children.Count; i++)
                    {
                        if (target!.Children[i].Node == child)
                        {
                            target!.Children.RemoveAt(i);
                            break;
                        }
                    }

                    target!._onChildrenChanged?.Invoke();
                });
            });

            if (node is InstanceGroup instanceGroup)
            {
                _onInstanceListChanged = instanceGroup.GetOnInstanceListChangedDelegate().Bind(() =>
                {
                    if (weakThis.TryGetTarget(out NodeViewModel? target))
                    {
                        target.ScheduleInstanceRefresh();
                    }
                });

                ScheduleInstanceRefresh();
            }
        }

        /// <summary>
        /// Prunes or restores this node from its parent's <see cref="Children"/> collection based on the current swatch filter.
        /// Removed nodes stay alive in <see cref="AllChildren"/> so they can be restored later. Must be called on the UI thread.
        /// </summary>
        public void SetFilteredOut(bool filteredOut)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (_isFilteredOut == filteredOut)
            {
                return;
            }

            IsFilteredOut = filteredOut;

            // Root nodes have no parent collection; the scene hierarchy manages them separately.
            if (_parent == null)
            {
                return;
            }

            NodeViewModel parent = _parent;

            if (filteredOut)
            {
                parent.Children.Remove(this);
            }
            else
            {
                // Re-insert at the position matching the visible sibling order. Count how many
                // currently-visible siblings come before this node in the full child list, so the
                // item lands in the right spot even when earlier siblings are still filtered out.
                int index = 0;

                foreach (NodeViewModel sibling in parent._allChildren)
                {
                    if (ReferenceEquals(sibling, this))
                    {
                        break;
                    }

                    if (!sibling.IsFilteredOut)
                    {
                        ++index;
                    }
                }

                parent.Children.Insert(index, this);
            }
        }
    }

    public sealed class InstanceNodeViewModel : NodeViewModel
    {
        public InstanceNodeViewModel(
            Node instanceHandle,
            NodeViewModel group,
            Action? onChildrenChanged,
            NodeViewModelIndex? index)
            : base(instanceHandle, group, onChildrenChanged, index)
        {
        }

        public string InstanceTooltip => $"Instance of {Parent?.DisplayName}";
    }

    /// <summary>
    /// Maps engine-side node identities onto the hierarchy's view models so a node coming back from
    /// the engine (a selection change, a focus change) can be resolved without walking the tree.
    /// </summary>
    public sealed class NodeViewModelIndex
    {
        private readonly Dictionary<UUID, NodeViewModel> _nodeViewModelsByUUID = new();

        public event Action? InstanceRowsChanged;

        public void NotifyInstanceRowsChanged()
        {
            InstanceRowsChanged?.Invoke();
        }

        public void Add(NodeViewModel nodeViewModel)
        {
            _nodeViewModelsByUUID[nodeViewModel.UUID] = nodeViewModel;

            foreach (NodeViewModel child in nodeViewModel.AllChildren)
            {
                Add(child);
            }
        }

        public void Remove(NodeViewModel nodeViewModel)
        {
            if (_nodeViewModelsByUUID.TryGetValue(nodeViewModel.UUID, out NodeViewModel? mapped) && ReferenceEquals(mapped, nodeViewModel))
            {
                _nodeViewModelsByUUID.Remove(nodeViewModel.UUID);
            }

            foreach (NodeViewModel child in nodeViewModel.AllChildren)
            {
                Remove(child);
            }
        }

        public NodeViewModel? Find(UUID uuid)
        {
            return _nodeViewModelsByUUID.TryGetValue(uuid, out NodeViewModel? nodeViewModel)
                ? nodeViewModel
                : null;
        }

        public void Clear()
        {
            _nodeViewModelsByUUID.Clear();
        }
    }
}
