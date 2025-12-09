using System;
using System;
using System.Collections.ObjectModel;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorViewModel : ViewModelBase
    {
        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

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
                    foreach (InspectorPropertyViewModelBase propertyViewModel in Properties)
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

            // sort by editorder attribute (if present), then by name
            List<Property> properties = nodeClass.Properties.OrderBy(p =>
            {
                ClassAttribute? attrEditOrder = p.GetAttribute("editorder");

                if (attrEditOrder != null)
                {
                    return attrEditOrder.Value.GetInt();
                }

                return int.MaxValue;
            }).ThenBy(p => p.Name.ToString()).ToList();

            foreach (Property property in properties)
            {
                try
                {
                    // skip non-editor properties
                    ClassAttribute? attrEditHide = property.GetAttribute("edithide");

                    if (attrEditHide != null && attrEditHide.Value.GetBool() == true)
                    {
                        continue;
                    }

                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }
                    Properties.Add(InspectorViewModelFactory.Create(SelectedNode, property, isReadOnly));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to create view model for property '{property.Name}': {ex.Message}");
                }
            }
        }
    }
}
