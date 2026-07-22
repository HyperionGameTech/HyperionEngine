using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Services;

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
                Dispatcher.UIThread.VerifyAccess();
                if (SetProperty(ref _selectedNode, value) && !_suppressSelectionNotifications)
                {
                    SelectedNodeChanged?.Invoke(_selectedNode?.Node);
                }
            }
        }

        public event Action<Node?>? SelectedNodeChanged;

        public ObservableCollection<NodeViewModel> SelectedNodes { get; } = new ObservableCollection<NodeViewModel>();

        public event Action? SelectionChanged;

        private Scene? _scene;
        public Scene? Scene => _scene;

        private DelegateHandler? _onSelectedNodeChanged;

        public void AttachToScene(Scene? scene)
        {
            Dispatcher.UIThread.VerifyAccess();

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
                Dispatcher.UIThread.Post(() =>
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
            Dispatcher.UIThread.VerifyAccess();

            _scene = null;
            RootNodes.Clear();

            _onSelectedNodeChanged?.Remove();
            _onSelectedNodeChanged = null;
        }

        public void SelectNodeFromEngine(Node? node)
        {
            Dispatcher.UIThread.VerifyAccess();

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

        public void UpdateSelectionFromEngine(IEnumerable<Node> nodes)
        {
            Dispatcher.UIThread.VerifyAccess();

            SelectedNodes.Clear();

            foreach (Node node in nodes)
            {
                if (node == null || !node.IsValid)
                {
                    continue;
                }

                NodeViewModel? viewModel = FindNodeViewModel(node.NativeAddress);

                if (viewModel != null)
                {
                    SelectedNodes.Add(viewModel);
                }
            }

            OnPropertyChanged(nameof(SelectedNodes));
        }

        public void ToggleNodeInSelection(NodeViewModel nodeViewModel)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (SelectedNodes.Contains(nodeViewModel))
            {
                SelectedNodes.Remove(nodeViewModel);
            }
            else
            {
                SelectedNodes.Add(nodeViewModel);
            }

            OnPropertyChanged(nameof(SelectedNodes));
            SelectionChanged?.Invoke();
        }

        public void ClearSelectedNodes()
        {
            Dispatcher.UIThread.VerifyAccess();

            SelectedNodes.Clear();
            OnPropertyChanged(nameof(SelectedNodes));
            SelectionChanged?.Invoke();
        }

        public List<NodeViewModel> GetFlattenedNodes()
        {
            List<NodeViewModel> result = new List<NodeViewModel>();
            foreach (NodeViewModel root in RootNodes)
            {
                FlattenRecursive(root, result);
            }
            return result;
        }

        private static void FlattenRecursive(NodeViewModel node, List<NodeViewModel> result)
        {
            result.Add(node);
            foreach (NodeViewModel child in node.Children)
            {
                FlattenRecursive(child, result);
            }
        }

        public void SetSuppressSelectionNotifications(bool suppress)
        {
            _suppressSelectionNotifications = suppress;
        }

        public void NotifySelectedNodesChanged()
        {
            OnPropertyChanged(nameof(SelectedNodes));
        }

        public bool IsRootNode(Node? node)
        {
            if (node == null || _scene == null)
            {
                return false;
            }

            Node? root = _scene.RootNode;
            return root != null && root.NativeAddress == node.NativeAddress;
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

        public bool ReparentNode(NodeViewModel dragged, NodeViewModel newParent)
        {
            if (dragged == null || newParent == null)
                return false;

            Node draggedNode = dragged.Node;
            Node newParentNode = newParent.Node;

            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                new Name("EditorCommandReparentNode"),
                draggedNode.NativeAddress.ToString(),
                newParentNode.NativeAddress.ToString());

            return true;
        }

        public void SetDropTarget(NodeViewModel? target)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (_currentDropTarget == target)
                return;

            if (_currentDropTarget != null)
                _currentDropTarget.IsDropTarget = false;

            _currentDropTarget = target;

            if (_currentDropTarget != null)
                _currentDropTarget.IsDropTarget = true;
        }

        private NodeViewModel? _currentDropTarget;

        public static bool IsAncestorOf(NodeViewModel potentialAncestor, NodeViewModel node)
        {
            // @NOTE Not thread safe currently, needs to be called on sim thread!

            NodeViewModel? current = node.Parent;
            while (current != null)
            {
                if (current == potentialAncestor)
                    return true;
                current = current.Parent;
            }
            return false;
        }
    }
}
