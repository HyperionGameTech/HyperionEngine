using System;
using System;
using System.Collections.ObjectModel;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorViewModel : ViewModelBase
    {
        public ObservableCollection<InspectorPropertyViewModel> Properties { get; } = new ObservableCollection<InspectorPropertyViewModel>();

        private Node? _selectedNode;
        public Node? SelectedNode
        {
            get => _selectedNode;
            private set => SetProperty(ref _selectedNode, value);
        }

        public void SetSelectedNode(Node? node)
        {
            if (SelectedNode != null && node != null && SelectedNode.IsValid && node.IsValid)
            {
                if (SelectedNode.NativeAddress == node.NativeAddress)
                {
                    foreach (InspectorPropertyViewModel propertyViewModel in Properties)
                    {
                        propertyViewModel.RefreshValue();
                    }

                    return;
                }
            }

            SelectedNode = node;
            RefreshProperties();
        }

        private void RefreshProperties()
        {
            Properties.Clear();
            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            Class nodeClass = SelectedNode.Class;

            Logger.Log(LogType.Debug, $"Inspector refreshing properties for node '{SelectedNode.Name}' of class '{nodeClass.Name}'");

            foreach (Property property in nodeClass.Properties.OrderBy(p => p.Name.ToString(), StringComparer.OrdinalIgnoreCase))
            {
                try
                {
                    Properties.Add(new InspectorPropertyViewModel(SelectedNode, property));

                    Logger.Log(LogType.Debug, $"Inspector added property view model for '{property.Name}'");
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to create view model for property '{property.Name}': {ex.Message}");
                }
            }
        }
    }
}
