using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorComponentViewModelBase : ViewModelBase
    {
        protected readonly Entity _target;

        public string Label { get; }

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new();
        public ObservableCollection<ComponentSubObjectViewModel> SubObjects { get; } = new();

        private bool _hasProperties;
        public bool HasProperties
        {
            get => _hasProperties;
            set => SetProperty(ref _hasProperties, value);
        }

        private bool _hasSubObjects;
        public bool HasSubObjects
        {
            get => _hasSubObjects;
            set => SetProperty(ref _hasSubObjects, value);
        }

        public InspectorComponentViewModelBase(Entity? target, string label)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            Label = label;
        }

        public virtual bool IsEditorVisible => true;

        public virtual void PopulateProperties()
        {
        }
    }

    public class InspectorComponentViewModel<T> : InspectorComponentViewModelBase where T : IComponent, allows ref struct
    {
        private static readonly Class? _componentClass = Class.GetClass(typeof(T));

        private static readonly bool _isEditorVisible = ComputeIsEditorVisible();

        private static bool ComputeIsEditorVisible()
        {
            if (_componentClass == null)
            {
                return true;
            }

            ClassAttribute? attrEditor = _componentClass.Value.GetAttribute("editor");

            return attrEditor == null || attrEditor.Value.GetBool();
        }

        public override bool IsEditorVisible => _isEditorVisible;

        public InspectorComponentViewModel(Entity? target)
            : base(target, GetLabel())
        {
        }

        private static string GetLabel()
        {
            if (_componentClass != null)
            {
                // Use the Label attribute if present
                ClassAttribute? attrLabel = _componentClass.Value.GetAttribute("label");

                if (attrLabel != null)
                {
                    return attrLabel.Value.GetString();
                }

                return _componentClass.Value.Name.ToString();
            }

            string typeName = typeof(T).Name;

            return typeName.EndsWith("Component", StringComparison.Ordinal)
                ? typeName
                : typeName + " Component";
        }

        public override void PopulateProperties()
        {
            if (_componentClass == null)
            {
                return;
            }

            Class cls = _componentClass.Value;
            IntPtr classAddress = cls.Address;
            TypeId componentTypeId = cls.TypeId;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    EntityManager? mgr = _target.EntityManager;

                    if (mgr == null)
                    {
                        return;
                    }

                    IntPtr componentPtr = mgr.GetComponentPtr(_target, componentTypeId);

                    if (componentPtr == IntPtr.Zero)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to get component pointer for '{typeof(T).Name}' on entity '{_target.Name}'");
                        return;
                    }

                    // Resolver: each time we need to read/write a component property, re-acquire the pointer
                    // to handle potential archetype relocation
                    Func<IntPtr> targetAddressResolver = () =>
                    {
                        EntityManager? m = _target.EntityManager;

                        if (m == null)
                        {
                            return IntPtr.Zero;
                        }

                        return m.GetComponentPtr(_target, componentTypeId);
                    };

                    List<Property> componentProperties = cls.Properties
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

                    List<InspectorPropertyViewModelBase> vms = new();

                    foreach (Property property in componentProperties)
                    {
                        try
                        {
                            TypeInfo typeInfo = property.TypeInfo;

                            bool isReadOnly = false;
                            ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                            if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                            {
                                isReadOnly = true;
                            }

                            vms.Add(InspectorViewModelFactory.CreateForComponent(
                                classAddress, targetAddressResolver, property, isReadOnly));
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(LogLevel.Debug, $"Inspector skipping component property '{property.Name}': {ex.Message}");
                        }
                    }

                    Dispatcher.UIThread.Post(() =>
                    {
                        Properties.Clear();
                        SubObjects.Clear();

                        foreach (var vm in vms)
                        {
                            Properties.Add(vm);
                        }

                        HasProperties = Properties.Count > 0;
                        HasSubObjects = false;
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to populate component properties for '{typeof(T).Name}': {ex.Message}");
                }
            });
        }
    }
}
