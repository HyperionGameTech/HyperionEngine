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
        private const string ParametersPropertyName = "Parameters";
        private const string NormalMapFlipYPropertyName = "NormalMapFlipY";
        private const string InverseHeightPropertyName = "InverseHeight";
        private const string RoughnessChannelPropertyName = "RoughnessChannel";
        private const string MetalnessChannelPropertyName = "MetalnessChannel";
        private const string AmbientOcclusionChannelPropertyName = "AmbientOcclusionChannel";

        private const string NormalsSlotName = "Normals";
        private const string ParallaxSlotName = "Parallax";
        private const string RoughnessSlotName = "Roughness";
        private const string MetalnessSlotName = "Metalness";
        private const string AmbientOcclusionSlotName = "AmbientOcclusion";

        private readonly Class _structClass;
        private readonly Property? _valuesProperty;

        private readonly Property? _parametersProperty;
        private Class? _parametersClass;
        private readonly Property? _flipYProperty;
        private readonly Property? _inverseHeightProperty;
        private readonly Property? _roughnessChannelProperty;
        private readonly Property? _metalnessChannelProperty;
        private readonly Property? _aoChannelProperty;

        private BoxedValue? _currentStructValue;
        private BoxedValue? _currentValuesArray;
        private BoxedValue? _currentParametersValue;

        public ObservableCollection<MaterialTextureSlotViewModel> Slots { get; } = new();

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

            (_parametersProperty, _flipYProperty, _inverseHeightProperty, _roughnessChannelProperty, _metalnessChannelProperty, _aoChannelProperty) = FindParametersProperties();
        }

        public MaterialTexturesPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _structClass = property.TypeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            _valuesProperty = FindValuesProperty();

            (_parametersProperty, _flipYProperty, _inverseHeightProperty, _roughnessChannelProperty, _metalnessChannelProperty, _aoChannelProperty) = FindParametersProperties();
        }

        public MaterialTexturesPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _structClass = typeInfo.Class!.Value;
            Value = _structClass.Name.ToString();
            _valuesProperty = FindValuesProperty();

            // No object context in this mode (bare getter/setter delegate) - inline controls stay unavailable.
            (_parametersProperty, _flipYProperty, _inverseHeightProperty, _roughnessChannelProperty, _metalnessChannelProperty, _aoChannelProperty)
                = (null, null, null, null, null, null);
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

        private (Property?, Property?, Property?, Property?, Property?, Property?) FindParametersProperties()
        {
            try
            {
                Property? parametersProperty = FindSiblingProperty(new Name(ParametersPropertyName));

                if (parametersProperty == null)
                {
                    return (null, null, null, null, null, null);
                }

                Class? parametersClass = parametersProperty.Value.TypeInfo.Class;

                if (parametersClass == null)
                {
                    return (null, null, null, null, null, null);
                }

                _parametersClass = parametersClass;

                Property? flipYProperty = parametersClass.Value.GetProperty(new Name(NormalMapFlipYPropertyName));
                Property? inverseHeightProperty = parametersClass.Value.GetProperty(new Name(InverseHeightPropertyName));
                Property? roughnessChannelProperty = parametersClass.Value.GetProperty(new Name(RoughnessChannelPropertyName));
                Property? metalnessChannelProperty = parametersClass.Value.GetProperty(new Name(MetalnessChannelPropertyName));
                Property? aoChannelProperty = parametersClass.Value.GetProperty(new Name(AmbientOcclusionChannelPropertyName));

                return (parametersProperty, flipYProperty, inverseHeightProperty, roughnessChannelProperty, metalnessChannelProperty, aoChannelProperty);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"MaterialTexturesPropertyViewModel: failed to find '{ParametersPropertyName}' property/accessors: {ex.Message}");

                return (null, null, null, null, null, null);
            }
        }

        private IntPtr GetCurrentParametersPointer() => Volatile.Read(ref _currentParametersValue)?.Pointer ?? IntPtr.Zero;

        private BoxedValue GetValuesArray()
        {
            return Volatile.Read(ref _currentValuesArray)
                ?? throw new InvalidOperationException("Textures value not yet loaded");
        }

        private BoxedValue GetSlotValue(int ordinal) => GetValuesArray().GetArrayElement(ordinal);

        private void SetSlotValue(int ordinal, BoxedValue value) => GetValuesArray().SetArrayElement(ordinal, value);

        private static void Replace(ref BoxedValue? field, BoxedValue? newValue)
        {
            BoxedValue? previous = Interlocked.Exchange(ref field, newValue);

            if (previous != null && !ReferenceEquals(previous, newValue))
            {
                previous.Dispose();
            }
        }

        /// <summary>
        /// Sim thread. Re-reads the Textures struct (and its Values array) from the material before
        /// a slot write, so setting one texture can't revert whatever else changed since the panel
        /// was built.
        /// </summary>
        private void ReloadTextures()
        {
            PreWriteCallback?.Invoke();

            try
            {
                BoxedValue structValue = GetPropertyValue();
                BoxedValue? valuesArray = _valuesProperty?.Get(_structClass.Address, structValue.Pointer);

                Replace(ref _currentStructValue, structValue);
                Replace(ref _currentValuesArray, valuesArray);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"MaterialTexturesPropertyViewModel: failed to re-read textures: {ex.Message}");
            }
        }

        /// <summary>
        /// Sim thread. Re-reads the sibling Parameters value before an inline control write. The
        /// same struct is also edited by the regular Parameters editor, so writing back a snapshot
        /// would revert whatever was changed there.
        /// </summary>
        private void ReloadParameters()
        {
            PreWriteCallback?.Invoke();

            if (_parametersProperty == null)
            {
                return;
            }

            try
            {
                Replace(ref _currentParametersValue, GetSiblingPropertyValue(_parametersProperty.Value));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"MaterialTexturesPropertyViewModel: failed to re-read parameters: {ex.Message}");
            }
        }

        private void WriteBack()
        {
            BoxedValue? structValue = Volatile.Read(ref _currentStructValue);
            BoxedValue? valuesArray = Volatile.Read(ref _currentValuesArray);

            if (structValue == null || _valuesProperty == null || valuesArray == null)
            {
                return;
            }

            _valuesProperty.Value.Set(_structClass.Address, structValue.Pointer, valuesArray);
            SetPropertyValue(structValue);
        }

        private void WriteBackParameters()
        {
            BoxedValue? parametersValue = Volatile.Read(ref _currentParametersValue);

            if (_parametersProperty == null || parametersValue == null)
            {
                return;
            }

            SetSiblingPropertyValue(_parametersProperty.Value, parametersValue);
        }

        private InspectorPropertyViewModelBase? BuildInlineControl(string slotLabel)
        {
            if (_parametersProperty == null || _parametersClass == null || Volatile.Read(ref _currentParametersValue) == null)
            {
                return null;
            }

            Property? property;
            string label;

            switch (slotLabel)
            {
                case NormalsSlotName:
                    property = _flipYProperty;
                    label = "Flip Y";
                    break;
                case ParallaxSlotName:
                    property = _inverseHeightProperty;
                    label = "Inverse Height";
                    break;
                case RoughnessSlotName:
                    property = _roughnessChannelProperty;
                    label = "Channel";
                    break;
                case MetalnessSlotName:
                    property = _metalnessChannelProperty;
                    label = "Channel";
                    break;
                case AmbientOcclusionSlotName:
                    property = _aoChannelProperty;
                    label = "Channel";
                    break;
                default:
                    return null;
            }

            if (property == null)
            {
                return null;
            }

            Class parametersClass = _parametersClass.Value;
            Property capturedProperty = property.Value;

            return InspectorViewModelFactory.CreateForValue(
                label,
                capturedProperty.TypeInfo,
                getter: () => capturedProperty.Get(parametersClass.Address, GetCurrentParametersPointer()),
                setter: v => capturedProperty.Set(parametersClass.Address, GetCurrentParametersPointer(), v),
                isReadOnly: _isReadOnly,
                depth: 0,
                initialize: false,
                preWriteCallback: ReloadParameters,
                postWriteCallback: WriteBackParameters,
                valueChangedCallback: () => ValueChangedCallback?.Invoke());
        }

        // arraySize is read on the sim thread; the boxed values are owned there and must not be
        // touched from here.
        private void BuildSlots(int arraySize)
        {
            Slots.Clear();

            if (_valuesProperty == null || arraySize <= 0)
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
                    preWriteCallback: ReloadTextures,
                    postWriteCallback: WriteBack,
                    valueChangedCallback: () => ValueChangedCallback?.Invoke());

                InspectorPropertyViewModelBase? inlineControl = BuildInlineControl(slotLabel);

                Slots.Add(new MaterialTextureSlotViewModel(vm, inlineControl));
            }

            HasSlots = Slots.Count > 0;
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                int arraySize;

                try
                {
                    BoxedValue newStructValue = GetPropertyValue();
                    BoxedValue? newValuesArray = _valuesProperty?.Get(_structClass.Address, newStructValue.Pointer);

                    BoxedValue? newParametersValue = _parametersProperty != null
                        ? GetSiblingPropertyValue(_parametersProperty.Value)
                        : null;

                    arraySize = newValuesArray?.GetArraySize() ?? 0;

                    Replace(ref _currentStructValue, newStructValue);
                    Replace(ref _currentValuesArray, newValuesArray);
                    Replace(ref _currentParametersValue, newParametersValue);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read Textures property '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                int capturedArraySize = arraySize;

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        // The slot list is derived from the texture key enum, so it only has to be
                        // built once. Rebuilding it on every refresh would tear down the asset
                        // pickers (and any pop-out panel) while the user is using them.
                        if (Slots.Count == 0)
                        {
                            BuildSlots(capturedArraySize);
                        }

                        foreach (MaterialTextureSlotViewModel slot in Slots)
                        {
                            slot.RefreshValue();
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
