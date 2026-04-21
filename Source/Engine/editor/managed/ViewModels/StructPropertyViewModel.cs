using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class StructPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const int MaxDepth = 4;

        private readonly int _depth;
        private readonly Class _structClass;

        private BoxedValue? _currentStructValue;

        public ObservableCollection<InspectorPropertyViewModelBase> SubProperties { get; } = new();

        private bool _hasSubProperties;
        public bool HasSubProperties
        {
            get => _hasSubProperties;
            private set => SetProperty(ref _hasSubProperties, value);
        }

        private bool _isExpanded = true;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        // The struct spans both label and value columns.
        public override bool ShowInlineLabel => false;

        public StructPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;
            _structClass = property.TypeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            InitializeSubProperties();
        }

        public StructPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;
            _structClass = property.TypeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            InitializeSubProperties();
        }

        public StructPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _depth = depth;
            _structClass = typeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            InitializeSubProperties();
        }

        // Returns the address of the struct copy held in _currentStructValue,
        // or IntPtr.Zero if not yet loaded (sub-prop calls will fail gracefully until first RefreshValue).
        private IntPtr GetCurrentStructPointer()
        {
            return _currentStructValue?.Pointer ?? IntPtr.Zero;
        }

        // Called by sub-property VMs via PostWriteCallback after they modify the struct copy.
        // Writes the entire (now-modified) struct copy back to the parent property.
        private void WriteStructToParent()
        {
            if (_currentStructValue == null)
            {
                return;
            }

            SetPropertyValue(_currentStructValue);
        }

        private void InitializeSubProperties()
        {
            if (_depth >= MaxDepth)
            {
                return;
            }

            List<Property> properties;

            try
            {
                properties = _structClass.Properties
                    .Where(p =>
                    {
                        ClassAttribute? attrEditHide = p.GetAttribute("edithide");
                        return attrEditHide == null || attrEditHide.Value.GetBool() == false;
                    })
                    .OrderBy(p =>
                    {
                        ClassAttribute? attrEditOrder = p.GetAttribute("editorder");
                        return attrEditOrder != null ? attrEditOrder.Value.GetInt() : int.MaxValue;
                    })
                    .ThenBy(p => p.Name.ToString())
                    .ToList();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"StructPropertyViewModel: Failed to enumerate properties for struct '{_structClass.Name}': {ex.Message}");
                return;
            }

            foreach (Property property in properties)
            {
                try
                {
                    bool isReadOnly = _isReadOnly;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    InspectorPropertyViewModelBase vm = InspectorViewModelFactory.CreateForComponent(
                        _structClass.Address,
                        GetCurrentStructPointer,
                        property,
                        isReadOnly,
                        _depth + 1,
                        initialize: false,
                        postWriteCallback: WriteStructToParent);

                    SubProperties.Add(vm);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Debug, $"StructPropertyViewModel: Skipping property '{property.Name}' for struct '{_structClass.Name}': {ex.Message}");
                }
            }

            HasSubProperties = SubProperties.Count > 0;
        }

        public override void RefreshValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    BoxedValue newStructValue = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        _currentStructValue = newStructValue;

                        foreach (var vm in SubProperties)
                        {
                            vm.RefreshValue();
                        }
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read struct property '{_property.Name}': {ex.Message}");
                }
            });
        }
    }
}
