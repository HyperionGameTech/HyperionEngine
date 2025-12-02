using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reflection;
using Avalonia.Threading;
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
            SelectedNode = node;
            RefreshProperties();
        }

        private void RefreshProperties()
        {
            Properties.Clear();
            if (SelectedNode == null) return;

            // Common properties
            Properties.Add(new InspectorPropertyViewModel("Name", SelectedNode.Name.ToString()));

            // Generic reflection for simple editable properties
            /// @TODO! Use Hyperion.Property and Hyperion.Field classes from the class itself.
            var props = SelectedNode.GetType()
                .GetProperties(BindingFlags.Public | BindingFlags.Instance)
                .Where(p => p.CanRead && p.CanWrite && IsSimpleType(p.PropertyType));

            foreach (var prop in props)
            {
                object? value = null;
                try { value = prop.GetValue(SelectedNode); } catch { }
                Properties.Add(new InspectorPropertyViewModel(prop.Name, value));
            }
        }

        private static bool IsSimpleType(Type t)
        {
            return t.IsPrimitive ||
                   t == typeof(string) ||
                   t == typeof(decimal) ||
                   t.IsEnum;
        }
    }
}
