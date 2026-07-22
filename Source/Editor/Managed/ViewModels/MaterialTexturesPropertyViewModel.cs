using System;
using System.Collections.ObjectModel;
using System.Threading;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class MaterialTexturesPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const string ValuesPropertyName = "Values";
        private const string TextureKeyEnumName = "MaterialTextureKey";

        private readonly Class _structClass;
        private readonly Property? _valuesProperty;

        private BoxedValue? _currentStructValue;
        private BoxedValue? _currentValuesArray;

        public ObservableCollection<InspectorPropertyViewModelBase> Slots { get; } = new();

        private bool _hasSlots;
        public bool HasSlots
        {
            get => _hasSlots;
            private set => SetProperty(ref _hasSlots, value);
        }

        private bool _isExpanded = true;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public override bool ShowInlineLabel => false;

        public MaterialTexturesPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _structClass = property.TypeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            _valuesProperty = FindValuesProperty();
        }

        public MaterialTexturesPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _structClass = property.TypeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            _valuesProperty = FindValuesProperty();
        }

        public MaterialTexturesPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _structClass = typeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            _valuesProperty = FindValuesProperty();
        }

        private Property? FindValuesProperty()
        {
            try
            {
                foreach (Property property in _structClass.Properties)
                {
                    if (property.Name.ToString() == ValuesPropertyName)
                    {
                        return property;
                    }
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"MaterialTexturesPropertyViewModel: failed to find '{ValuesPropertyName}' property on '{_structClass.Name}': {ex.Message}");
            }

            return null;
        }

        private IntPtr GetCurrentStructPointer() => _currentStructValue?.Pointer ?? IntPtr.Zero;

        private BoxedValue GetValuesArray()
        {
            if (_currentValuesArray == null)
            {
                throw new InvalidOperationException("Textures value not yet loaded");
            }

            return _currentValuesArray;
        }

        private BoxedValue GetSlotValue(int ordinal) => GetValuesArray().GetArrayElement(ordinal);

        private void SetSlotValue(int ordinal, BoxedValue value) => GetValuesArray().SetArrayElement(ordinal, value);

        private void WriteBack()
        {
            if (_currentStructValue == null || _valuesProperty == null || _currentValuesArray == null)
            {
                return;
            }

            _valuesProperty.Value.Set(_structClass.Address, GetCurrentStructPointer(), _currentValuesArray);
            SetPropertyValue(_currentStructValue);
        }

        private void BuildSlots()
        {
            Slots.Clear();

            if (_valuesProperty == null || _currentValuesArray == null)
            {
                HasSlots = false;
                return;
            }

            Class? keyEnumClass = Class.TryGetClass(TextureKeyEnumName);

            if (keyEnumClass == null)
            {
                Logger.Log(LogLevel.Warning, $"MaterialTexturesPropertyViewModel: could not resolve enum class '{TextureKeyEnumName}'");
                HasSlots = false;
                return;
            }

            TypeInfo elementTypeInfo = _valuesProperty.Value.TypeInfo.GetElementTypeInfo();
            int arraySize = _currentValuesArray.GetArraySize();

            foreach (StaticField staticField in keyEnumClass.Value.StaticFields)
            {
                ulong rawValue;

                try
                {
                    object? boxedEnumValue = staticField.ReadObject();

                    if (boxedEnumValue == null)
                    {
                        continue;
                    }

                    rawValue = Convert.ToUInt64(boxedEnumValue);
                }
                catch
                {
                    continue;
                }

                if (rawValue == 0)
                {
                    continue; // NONE
                }

                int ordinal = (int)Math.Log2(rawValue);

                if (ordinal < 0 || ordinal >= arraySize)
                {
                    continue;
                }

                int capturedOrdinal = ordinal;
                string slotLabel = staticField.Name.ToString();

                InspectorPropertyViewModelBase vm = InspectorViewModelFactory.CreateForValue(
                    slotLabel,
                    elementTypeInfo,
                    getter: () => GetSlotValue(capturedOrdinal),
                    setter: v => SetSlotValue(capturedOrdinal, v),
                    isReadOnly: _isReadOnly,
                    depth: 0,
                    initialize: false,
                    postWriteCallback: WriteBack);

                Slots.Add(vm);
            }

            HasSlots = Slots.Count > 0;
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
                    BoxedValue? newValuesArray = _valuesProperty?.Get(_structClass.Address, newStructValue.Pointer);

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        _currentStructValue = newStructValue;
                        _currentValuesArray = newValuesArray;

                        BuildSlots();

                        foreach (var vm in Slots)
                        {
                            vm.RefreshValue();
                        }
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read Textures property '{Label}': {ex.Message}");
                }
            });
        }
    }
}
