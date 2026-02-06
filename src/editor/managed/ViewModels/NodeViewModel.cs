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
            set
            {
                if (SetProperty(ref _name, value))
                {
                    try
                    {
                        _node.Name = new Name(value);
                    }
                    catch
                    {
                        Logger.Log(LogType.Error, $"Failed to set node name to '{value}'.");
                    }

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

        public ObservableCollection<NodeViewModel> Children { get; } = new ObservableCollection<NodeViewModel>();

        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public bool IsDirty => _node.Dirty;

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
            for (int i = 0; i < node.NumChildren(); i++)
            {
                Node? child = node.GetChild(i);

                if (child != null)
                {
                    Children.Add(new NodeViewModel(child, this));
                }
            }

            WeakReference<NodeViewModel> weakThis = new WeakReference<NodeViewModel>(this);

            // Subscribe to child added/removed if available
            _onChildAdded = node.GetOnChildAddedDelegate().Bind((Node child, bool isDirect) =>
            {
                NodeViewModel? target;
                if (!weakThis.TryGetTarget(out target))
                {
                    Logger.Log(LogType.Warn, "NodeViewModel target has been garbage collected before child added handler could be invoked.");
                    return;
                }

                Dispatcher.UIThread.Post(() => target!.Children.Add(new NodeViewModel(child, target)));
            });

            _onChildRemoved = node.GetOnChildRemovedDelegate().Bind((Node child, bool isDirect) =>
            {
                NodeViewModel? target;
                if (!weakThis.TryGetTarget(out target))
                {
                    Logger.Log(LogType.Warn, "NodeViewModel target has been garbage collected before child removed handler could be invoked.");
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
