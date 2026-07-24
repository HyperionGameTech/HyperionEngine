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

        public ObservableCollection<NodeViewModel> Children { get; } = new ObservableCollection<NodeViewModel>();

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

        // @TODO
        public bool IsDirty => false;//_node.Dirty;

        private DelegateHandler? _onChildAdded;
        private DelegateHandler? _onChildRemoved;

        public NodeViewModel(Node node, NodeViewModel? parent = null)
        {
            _node = node;
            _parent = parent;
            _name = node.Name.ToString();
            
            // Root nodes are expanded by default
            _isExpanded = parent == null;

            // Initialize existing children
            for (uint i = 0; i < node.NumChildren(); i++)
            {
                Node? child = node.GetChild(i);

                if (child != null)
                {
                    Children.Add(new NodeViewModel(child, this));
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

                Dispatcher.UIThread.Post(() => target!.Children.Add(new NodeViewModel(child, target)));
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
                    for (int i = 0; i < target!.Children.Count; i++)
                    {
                        if (target!.Children[i].Node == child)
                        {
                            target!.Children.RemoveAt(i);
                            break;
                        }
                    }
                });
            });
        }
    }
}
