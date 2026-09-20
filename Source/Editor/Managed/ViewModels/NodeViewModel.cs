using System.Collections.ObjectModel;
using Avalonia.Threading;
using Avalonia.Media;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class NodeViewModel : ViewModelBase
    {
        private readonly Node _node;
        private readonly NodeViewModel? _parent;

        public Node Node => _node;
        public NodeViewModel? Parent => _parent;

        public UUID UUID => _node.UUID;

        public bool IsRootNode => _parent == null;

        public bool CanMoveToGrandparent => _parent?.Parent != null
            // Doesn't make sense to reparent an InstancedMeshProxy.
            && _node.Class.Name != new Name(nameof(InstancedMeshProxy), weak: true);

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
        public FontStyle NameFontStyle => IsUnnamed ? FontStyle.Italic : FontStyle.Normal;

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
            DirectionalLight    => "Lightbulb",
            PointLight          => "Lightbulb",
            SpotLight           => "Lightbulb",
            AreaRectLight       => "HorizontalRule",
            Camera              => "DeviceCamera",
            ReflectionProbe     => "Globe",
            ParticleVolume      => "Sparkle",
            InstancedMeshProxy  => "Combine",
            Bone                => "GitBranch",
            VolumeBase          => "Package",
            Entity              => "CircleLarge",
            _                   => "Circle",
        };

        private string _sourcePrefabName;

        public bool IsPrefabInstance => _sourcePrefabName.Length != 0;
        public string SourcePrefabName => _sourcePrefabName;
        public string SavePrefabHeader => $"Save Prefab '{_sourcePrefabName}'";
        public string PrefabInstanceTooltip => $"Prefab instance: {_sourcePrefabName}";

        public void RefreshSourcePrefab()
        {
            Dispatcher.UIThread.VerifyAccess();

            _sourcePrefabName = EngineManager.EditorGame?.EditorSubsystem?.GetSourcePrefabName(_node) ?? string.Empty;

            OnPropertyChanged(nameof(IsPrefabInstance));
            OnPropertyChanged(nameof(SourcePrefabName));
            OnPropertyChanged(nameof(SavePrefabHeader));
            OnPropertyChanged(nameof(PrefabInstanceTooltip));
        }

        private readonly List<NodeViewModel> _allChildren = new List<NodeViewModel>();

        public ObservableCollection<NodeViewModel> Children { get; } = new ObservableCollection<NodeViewModel>();

        public IReadOnlyList<NodeViewModel> AllChildren => _allChildren;

        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        public ObservableCollection<MoveToSceneItemViewModel> MoveToSceneTargets { get; } = new ObservableCollection<MoveToSceneItemViewModel>();

        private bool _hasMoveToSceneTargets;
        public bool HasMoveToSceneTargets
        {
            get => _hasMoveToSceneTargets;
            private set => SetProperty(ref _hasMoveToSceneTargets, value);
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

        /// <summary>
        /// Whether either collision action applies to this node, so the submenu can hide entirely for
        /// nodes that are not mesh entities.
        /// </summary>
        public bool HasCollisionActions => CanGenerateCollision || CanFitCollisionToMesh;

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

                    OnPropertyChanged(nameof(HasCollisionActions));
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
            FitVolumeToSelectionHeader = fitTargetCount > 1 ? $"Fit to {fitTargetCount} Selected Nodes" : "Fit to Selected Node";

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

        public void RefreshActions()
        {
            Dispatcher.UIThread.VerifyAccess();

            Actions.Clear();

            foreach (InspectorActionViewModel actionVm in InspectorActionsHelper.GetActions(_node))
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

            if (mainWindowViewModel != null && !IsRootNode)
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

        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
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

        // @TODO
        public bool IsDirty => false;//_node.Dirty;

        private DelegateHandler? _onChildAdded;
        private DelegateHandler? _onChildRemoved;

        private readonly Action? _onChildrenChanged;
        private readonly NodeViewModelIndex? _index;

        /// <summary>
        /// Walks the node's subtree and binds its child handlers, so this must run on the thread that mutates the
        /// hierarchy (the sim thread). Register the result with <see cref="NodeViewModelIndex.Add"/> on the UI thread.
        /// </summary>
        public NodeViewModel(Node node, NodeViewModel? parent = null, Action? onChildrenChanged = null, NodeViewModelIndex? index = null)
        {
            _node = node;
            _parent = parent;
            _onChildrenChanged = onChildrenChanged;
            _index = index;
            _name = node.Name.ToString();

            _sourcePrefabName = EngineManager.EditorGame?.EditorSubsystem?.GetSourcePrefabName(node) ?? string.Empty;

            // Root nodes are expanded by default
            _isExpanded = parent == null;

            // Initialize existing children
            for (uint i = 0; i < node.NumChildren(); i++)
            {
                Node? child = node.GetChild(i);

                if (child != null)
                {
                    NodeViewModel childViewModel = new NodeViewModel(child, this, onChildrenChanged, index);

                    _allChildren.Add(childViewModel);
                    Children.Add(childViewModel);
                }
            }

            WeakReference<NodeViewModel> weakThis = new WeakReference<NodeViewModel>(this);

            _onChildAdded = node.GetOnChildAddedDelegate().Bind((Node child, bool isDirect) =>
            {
                if (!isDirect)
                    return;

                NodeViewModel? target;
                if (!weakThis.TryGetTarget(out target))
                {
                    Logger.Log(LogLevel.Warning, "NodeViewModel target has been garbage collected before child added handler could be invoked.");
                    return;
                }

                // built inline on the thread attaching the child; its subtree may be mutated again before the UI gets to it
                NodeViewModel childViewModel = new NodeViewModel(child, target, target!._onChildrenChanged, target!._index);

                Dispatcher.UIThread.Post(() =>
                {
                    target!._index?.Add(childViewModel);
                    target!._allChildren.Add(childViewModel);
                    target!.Children.Add(childViewModel);

                    target!._onChildrenChanged?.Invoke();
                });
            });

            _onChildRemoved = node.GetOnChildRemovedDelegate().Bind((Node child, bool isDirect) =>
            {
                if (!isDirect)
                    return;

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
        }

        /// <summary>
        /// Prunes or restores this node from its parent's <see cref="Children"/> collection based on the
        /// current swatch filter. Removed nodes stay alive in <see cref="AllChildren"/> so they can be
        /// restored later. Must be called on the UI thread.
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

    /// <summary>
    /// Maps engine-side node identities onto the hierarchy's view models so a node coming back from
    /// the engine (a selection change, a focus change) can be resolved without walking the tree.
    /// </summary>
    public sealed class NodeViewModelIndex
    {
        private readonly Dictionary<UUID, NodeViewModel> _nodeViewModelsByUUID = new();

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
            return _nodeViewModelsByUUID.TryGetValue(uuid, out NodeViewModel? nodeViewModel) ? nodeViewModel : null;
        }

        public void Clear()
        {
            _nodeViewModelsByUUID.Clear();
        }
    }
}
