using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ComponentSubObjectViewModel : ViewModelBase
    {
        public string Label { get; }
        public ObjectBase Target { get; }
        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new();

        private readonly Action? _postWriteCallback;

        private bool _hasProperties;
        public bool HasProperties
        {
            get => _hasProperties;
            private set => SetProperty(ref _hasProperties, value);
        }

        public ComponentSubObjectViewModel(string label, ObjectBase target, int depth = 0, Action? postWriteCallback = null)
        {
            Label = label;
            Target = target ?? throw new ArgumentNullException(nameof(target));
            _postWriteCallback = postWriteCallback;

            PopulateProperties(depth);
        }

        private void PopulateProperties(int depth)
        {
            if (!Target.IsValid)
            {
                return;
            }

            Class cls = Target.Class;

            List<Property> properties = cls.Properties
                .Where(p =>
                {
                    ClassAttribute? attrEditHide = p.GetAttribute("edithide");

                    if (attrEditHide != null && attrEditHide.Value.GetBool() == true)
                    {
                        return false;
                    }

                    return true;
                })
                .OrderBy(p =>
                {
                    ClassAttribute? attrEditOrder = p.GetAttribute("editorder");

                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }

                    return int.MaxValue;
                })
                .ThenBy(p => p.Name.ToString())
                .ToList();

            foreach (Property property in properties)
            {
                try
                {
                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    InspectorPropertyViewModelBase vm = InspectorViewModelFactory.Create(Target, property, isReadOnly, depth, _postWriteCallback);

                    Properties.Add(vm);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for sub-object property '{property.Name}': {ex.Message}");
                }
            }

            HasProperties = Properties.Count > 0;
        }
    }
}
