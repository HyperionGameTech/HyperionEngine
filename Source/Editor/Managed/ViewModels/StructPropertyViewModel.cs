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

        // Sim-thread-owned copy of the struct. Sub-property view models read and write through this
        // copy and then it is written back to the parent as a whole.
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
            return Volatile.Read(ref _currentStructValue)?.Pointer ?? IntPtr.Zero;
        }

        /// <summary>
        /// Called on the sim thread by sub-property view models before they modify the struct copy.
        /// Re-reads the struct from the parent first (chaining up through the containing struct),
        /// so a write applies to the object's current value rather than to a snapshot taken when
        /// the panel was built.
        /// </summary>
        private void ReloadStructFromParent()
        {
            PreWriteCallback?.Invoke();

            try
            {
                ReplaceStructValue(GetPropertyValue());
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"StructPropertyViewModel: failed to re-read struct '{Label}': {ex.Message}");
            }
        }

        // Called by sub-property VMs via PostWriteCallback after they modify the struct copy.
        // Writes the entire (now-modified) struct copy back to the parent property.
        private void WriteStructToParent()
        {
            BoxedValue? current = Volatile.Read(ref _currentStructValue);

            if (current == null)
            {
                return;
            }

            SetPropertyValue(current);
        }

        // Replaces the cached copy and disposes the previous one on the calling (sim) thread.
        // Letting the finalizer do it would release engine handles from the GC thread.
        private void ReplaceStructValue(BoxedValue? newValue)
        {
            BoxedValue? previous = Interlocked.Exchange(ref _currentStructValue, newValue);

            if (previous != null && !ReferenceEquals(previous, newValue))
            {
                previous.Dispose();
            }
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
                        ClassAttribute? attrEditor = p.GetAttribute("editor");

                        return attrEditor == null || attrEditor.Value.GetBool() == true;
                    })
                    .OrderBy(p =>
                    {
                        ClassAttribute? attrEditOrder = p.GetAttribute("editororder");
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
                        preWriteCallback: ReloadStructFromParent,
                        postWriteCallback: WriteStructToParent,
                        valueChangedCallback: () => ValueChangedCallback?.Invoke());

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
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    ReplaceStructValue(GetPropertyValue());
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read struct property '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        foreach (InspectorPropertyViewModelBase vm in SubProperties)
                        {
                            vm.RefreshValue();
                        }
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }
    }
}
