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
        public NodeViewModel? SelectedNode
        {
            get => _selectedNode;
            set
            {
                if (SetProperty(ref _selectedNode, value))
                {
                    SelectedNodeChanged?.Invoke(_selectedNode?.Node);
                }
            }
        }

        public event Action<Node?>? SelectedNodeChanged;

        private Scene? _scene;

        public void AttachToScene(Scene? scene)
        {
            RootNodes.Clear();

            if (scene == null)
            {
                return;
            }

            Node? root = scene.RootNode;
            if (root != null)
            {
                RootNodes.Add(new NodeViewModel(root));
            }

            // React to root changes
            scene.GetOnRootNodeChangedDelegate().Bind((Node newRoot, Node oldRoot) =>
            {
                Dispatcher.UIThread.Invoke(() =>
                {
                    RootNodes.Clear();
                    if (newRoot != null)
                    {
                        RootNodes.Add(new NodeViewModel(newRoot));
                    }
                });
            }).Detach();
        }
    }
}
