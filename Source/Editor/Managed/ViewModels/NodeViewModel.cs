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
            DirectionalLight    => "Sun",
            PointLight          => "Lightbulb",
            SpotLight           => "Spotlight",
            AreaRectLight       => "RectangleHorizontal",
            Camera              => "Video",
            ReflectionProbe     => "Orbit",
            ParticleVolume      => "Sparkles",
            InstancedMeshProxy  => "SquaresUnite",
            Bone                => "Bone",
            VolumeBase          => "Box",
            Entity              => "Shapes",
            _                   => "Circle",
        };

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

        public NodeViewModel(Node node, NodeViewModel? parent = null, Action? onChildrenChanged = null)
        {
            _node = node;
            _parent = parent;
            _onChildrenChanged = onChildrenChanged;
            _name = node.Name.ToString();
            
            // Root nodes are expanded by default
            _isExpanded = parent == null;

            // Initialize existing children
            for (uint i = 0; i < node.NumChildren(); i++)
            {
                Node? child = node.GetChild(i);

                if (child != null)
                {
                    NodeViewModel childViewModel = new NodeViewModel(child, this, onChildrenChanged);

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

                Dispatcher.UIThread.Post(() =>
                {
                    NodeViewModel childViewModel = new NodeViewModel(child, target, target!._onChildrenChanged);

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
        /// current layer filter. Removed nodes stay alive in <see cref="AllChildren"/> so they can be
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
}
