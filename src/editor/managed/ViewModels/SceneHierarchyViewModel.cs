using System;
using System.Collections.ObjectModel;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class SceneHierarchyViewModel : ViewModelBase
    {
        public ObservableCollection<NodeViewModel> RootNodes { get; } = new ObservableCollection<NodeViewModel>();

        private NodeViewModel? _selectedNode;
        private bool _suppressSelectionNotifications;
        public NodeViewModel? SelectedNode
        {
            get => _selectedNode;
            set
            {
                Dispatcher.UIThread.CheckAccess();
                if (SetProperty(ref _selectedNode, value) && !_suppressSelectionNotifications)
                {
                    SelectedNodeChanged?.Invoke(_selectedNode?.Node);
                }
            }
        }

        public event Action<Node?>? SelectedNodeChanged;

        private Scene? _scene;
        private DelegateHandler? _onSelectedNodeChanged;

        public void AttachToScene(Scene? scene)
        {
            Dispatcher.UIThread.CheckAccess();

            _scene = scene;
            RootNodes.Clear();

            _suppressSelectionNotifications = true;
            try
            {
                SelectedNode = null;
            }
            finally
            {
                _suppressSelectionNotifications = false;
            }

            if (scene == null)
            {
                return;
            }

            Node? root = scene.RootNode;
            if (root != null)
            {
                RootNodes.Add(new NodeViewModel(root));
            }
            
            _onSelectedNodeChanged?.Remove();
            _onSelectedNodeChanged = scene.GetOnRootNodeChangedDelegate().Bind((Node newRoot, Node oldRoot) =>
            {
                Dispatcher.UIThread.Invoke(() =>
                {
                    _scene = scene;
                    RootNodes.Clear();
                    if (newRoot != null)
                    {
                        RootNodes.Add(new NodeViewModel(newRoot));
                    }
                });
            });
        }

        void DetachFromScene()
        {
            Dispatcher.UIThread.CheckAccess();

            _scene = null;
            RootNodes.Clear();

            _onSelectedNodeChanged?.Remove();
            _onSelectedNodeChanged = null;
        }

        public void SelectNodeFromEngine(Node? node)
        {
            Dispatcher.UIThread.CheckAccess();

            _suppressSelectionNotifications = true;

            try
            {
                if (node == null || !node.IsValid)
                {
                    SelectedNode = null;
                    return;
                }

                NodeViewModel? viewModel = FindNodeViewModel(node.NativeAddress);

                if (viewModel != null)
                {
                    ExpandAncestors(viewModel);
                }

                SelectedNode = viewModel;
            }
            finally
            {
                _suppressSelectionNotifications = false;
            }
        }

        private static void ExpandAncestors(NodeViewModel node)
        {
            node.IsExpanded = true;

            NodeViewModel? current = node.Parent;
            while (current != null)
            {
                current.IsExpanded = true; /// \todo Make it expand the tree node in UI!!
                current = current.Parent;
            }
        }

        private NodeViewModel? FindNodeViewModel(IntPtr nativeAddress)
        {
            foreach (NodeViewModel root in RootNodes)
            {
                NodeViewModel? result = FindNodeViewModelRecursive(root, nativeAddress);
                if (result != null)
                {
                    return result;
                }
            }

            return null;
        }

        private static NodeViewModel? FindNodeViewModelRecursive(NodeViewModel nodeViewModel, IntPtr nativeAddress)
        {
            if (nodeViewModel.Node != null && nodeViewModel.Node.NativeAddress == nativeAddress)
            {
                return nodeViewModel;
            }

            foreach (NodeViewModel child in nodeViewModel.Children)
            {
                NodeViewModel? found = FindNodeViewModelRecursive(child, nativeAddress);
                if (found != null)
                {
                    return found;
                }
            }

            return null;
        }
    }
}
