using System.Collections.ObjectModel;
using Avalonia.Threading;
using Avalonia.Media;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class NodeViewModel : ViewModelBase
    {
        private readonly Node _node;
        public Node Node => _node;

        private string _name;
        public string Name
        {
            get => _name;
            set
            {
                if (SetProperty(ref _name, value))
                {
                    try { _node.Name = new Name(value); } catch { }
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

        public NodeViewModel(Node node)
        {
            _node = node;
            _name = node.Name.ToString();

            // Initialize existing children
            for (int i = 0; i < node.NumChildren(); i++)
            {
                Node? child = node.GetChild(i);

                if (child != null)
                {
                    Children.Add(new NodeViewModel(child));
                }
            }

            // Subscribe to child added/removed if available
            node.GetOnChildAddedDelegate().Bind((Node child, bool isDirect) =>
            {
                Dispatcher.UIThread.Invoke(() => Children.Add(new NodeViewModel(child)));
            }).Detach();

            node.GetOnChildRemovedDelegate().Bind((Node child, bool isDirect) =>
            {
                Dispatcher.UIThread.Invoke(() =>
                {
                    for (int i = 0; i < Children.Count; i++)
                    {
                        if (Children[i].Node == child)
                        {
                            Children.RemoveAt(i);
                            break;
                        }
                    }
                });
            }).Detach();
        }
    }
}
