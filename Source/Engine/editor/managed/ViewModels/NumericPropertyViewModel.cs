using System;
using System.Globalization;
using System.Threading;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class NumericPropertyViewModel : InspectorPropertyViewModelBase
    {
        private readonly string _typeName;
        private string _editableValue = string.Empty;

        public NumericPropertyViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
            _typeName = property.TypeInfo.Name.ToString();
        }

        public NumericPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _typeName = property.TypeInfo.Name.ToString();
        }

        public NumericPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _typeName = typeInfo.Name.ToString();
        }

        public override bool IsNumericEditable => true;

        public string EditableValue
        {
            get => _editableValue;
            set => SetProperty(ref _editableValue, value);
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
                    using BoxedValue boxed = GetPropertyValue();
                    object? rawValue = boxed.GetValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        Value = rawValue?.ToString() ?? string.Empty;
                        EditableValue = rawValue?.ToString() ?? string.Empty;
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
                }
            });
        }

        public override void CommitValue()
        {
            CommitNumericValue(_editableValue);
        }

        private void CommitNumericValue(string text)
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            object? boxedObject = ParseToType(text, _typeName);

            if (boxedObject == null)
            {
                _isRefreshing = 0;
                return;
            }

            object captured = boxedObject;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(captured);
                    CommitPropertyChange($"Set {Label}", boxed);

                    Dispatcher.UIThread.Post(() => _isRefreshing = 0);
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to write property '{_property.Name}': {ex.Message}");
                }
            });
        }

        private static object? ParseToType(string text, string typeName)
        {
            if (string.IsNullOrWhiteSpace(text))
            {
                return null;
            }

            switch (typeName)
            {
                case "float":
                    return float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out float fVal) ? fVal : (object?)null;
                case "double":
                    return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double dVal) ? dVal : (object?)null;
                case "bool":
                    if (bool.TryParse(text, out bool bVal)) return bVal;
                    if (text == "1") return true;
                    if (text == "0") return false;
                    return null;
                case "int8":
                    return sbyte.TryParse(text, out sbyte i8) ? i8 : (object?)null;
                case "uint8":
                    return byte.TryParse(text, out byte u8) ? u8 : (object?)null;
                case "int16":
                    return short.TryParse(text, out short i16) ? i16 : (object?)null;
                case "uint16":
                    return ushort.TryParse(text, out ushort u16) ? u16 : (object?)null;
                case "int32":
                case "int":
                    return int.TryParse(text, out int i32) ? i32 : (object?)null;
                case "uint32":
                case "uint":
                    return uint.TryParse(text, out uint u32) ? u32 : (object?)null;
                case "int64":
                case "long":
                    return long.TryParse(text, out long i64) ? i64 : (object?)null;
                case "uint64":
                case "ulong":
                    return ulong.TryParse(text, out ulong u64) ? u64 : (object?)null;
                default:
                    // Fallback: try int then double
                    if (long.TryParse(text, out long fallbackInt)) return fallbackInt;
                    if (double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double fallbackFloat)) return fallbackFloat;
                    return null;
            }
        }
    }
}
