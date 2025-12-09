using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorPropertyViewModel : ViewModelBase
    {
        private readonly ObjectBase _target;
        private readonly Property _property;
        private readonly bool _isStringProperty;
        private readonly bool _isNameProperty;
        private readonly bool _isEnumProperty;
        private readonly bool _isEnumFlagsProperty;

        private readonly Class? _class;

        private string _value = string.Empty;
        private string _editableValue = string.Empty;
        private List<KeyValuePair<string, object?>>? _enumValues;
        private List<EnumFlagEntry>? _enumFlagEntries;
        private object? _selectedEnumValue;
        private bool _isRefreshing;

        private static readonly Dictionary<uint, Type?> EnumTypeCache = new Dictionary<uint, Type?>();

        public Property Property => _property;

        public string Name => _property.Name.ToString();

        public string Value
        {
            get => _value;
            private set => SetProperty(ref _value, value);
        }

        public string EditableValue
        {
            get => _editableValue;
            set
            {
                if (SetProperty(ref _editableValue, value) && !_isRefreshing && IsTextEditable)
                {
                    CommitEditableText(value);
                }
            }
        }

        public bool IsStringEditable => _isStringProperty;
        public bool IsNameEditable => _isNameProperty;
        public bool IsTextEditable => _isStringProperty || _isNameProperty;
        public bool IsEnumEditable => _isEnumProperty && EnumValues != null && EnumValues.Count > 0;
        public bool IsEnumFlagsEditable => _isEnumFlagsProperty && EnumFlagEntries != null && EnumFlagEntries.Count > 0;
        public bool ShowTextValue => !IsTextEditable && !IsEnumEditable && !IsEnumFlagsEditable;

        public List<KeyValuePair<string, object?>>? EnumValues
        {
            get => _enumValues;
            private set => SetProperty(ref _enumValues, value);
        }

        public List<EnumFlagEntry>? EnumFlagEntries
        {
            get => _enumFlagEntries;
            private set => SetProperty(ref _enumFlagEntries, value);
        }

        public object? SelectedEnumValue
        {
            get => _selectedEnumValue;
            set
            {
                if (SetProperty(ref _selectedEnumValue, value) && !_isRefreshing && _isEnumProperty)
                {
                    CommitEnumValue(value);
                }
            }
        }

        public InspectorPropertyViewModel(ObjectBase target, Property property)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _property = property;

            TypeInfo typeInfo = property.TypeInfo;

            _isStringProperty = typeInfo.IsString;
            _isNameProperty = IsNameType(typeInfo);

            _class = typeInfo.Class;

            if (_class is Class typeInfoClass)
            {
                Logger.Log(LogType.Debug, $"Inspector property '{Name}' has type class '{typeInfoClass.Name}', type info name: '{typeInfo.Name}'");

                if (typeInfo.IsEnumFlags)
                {
                    _isEnumFlagsProperty = true;
                    _isEnumProperty = true; // still an enum

                    List<EnumFlagEntry> entries = new List<EnumFlagEntry>();

                    foreach (StaticField staticField in typeInfoClass.StaticFields)
                    {
                        try
                        {
                            object? flagValue = staticField.ReadObject();
                            entries.Add(new EnumFlagEntry(staticField.Name.ToString(), flagValue, OnFlagEntryChanged));
                            Logger.Log(LogType.Debug, $"Inspector added enum flag static field '{staticField.Name}' to enum flag values for property '{Name}'");
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(LogType.Warn, $"Inspector failed to read enum flag static field '{staticField.Name}': {ex.Message}");
                        }
                    }

                    EnumFlagEntries = entries;
                }
                else if (typeInfoClass.IsEnumType)
                {
                    _isEnumProperty = true;

                    // use StaticFields of the enum class to populate EnumValues
                    List<KeyValuePair<string, object?>> enumValuesList = new List<KeyValuePair<string, object?>>();

                    foreach (StaticField staticField in typeInfoClass.StaticFields)
                    {
                        try
                        {
                            enumValuesList.Add(new KeyValuePair<string, object?>(staticField.Name.ToString(), staticField.ReadObject()));
                            Logger.Log(LogType.Debug, $"Inspector added enum static field '{staticField.Name}' to enum values for property '{Name}'");
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(LogType.Warn, $"Inspector failed to read enum static field '{staticField.Name}': {ex.Message}");
                        }
                    }

                    EnumValues = enumValuesList;
                }
            }

            RefreshValue();
        }

        public void RefreshValue()
        {
            _isRefreshing = true;

            try
            {
                if (!_target.IsValid)
                {
                    Value = "(invalid target)";
                    if (IsTextEditable)
                    {
                        EditableValue = string.Empty;
                    }

                    return;
                }

                using HypData data = _property.Get(_target);
                object? rawValue = data.GetValue();
                string formattedValue = FormatValue(rawValue);

                Value = formattedValue;

                if (IsTextEditable)
                {
                    EditableValue = rawValue?.ToString() ?? string.Empty;
                }
                else
                {
                    EditableValue = formattedValue;
                }

                if (_isEnumFlagsProperty)
                {
                    UpdateFlagSelectionsFromValue(rawValue);
                }
                else if (_isEnumProperty)
                {
                    SelectedEnumValue = rawValue;
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");
                Value = "(unavailable)";

                if (IsTextEditable)
                {
                    EditableValue = string.Empty;
                }
            }
            finally
            {
                _isRefreshing = false;
            }
        }

        private void CommitEditableText(string value)
        {
            if (!_target.IsValid)
            {
                return;
            }

            try
            {
                using HypData data = _isNameProperty
                    ? new HypData(new Name(value ?? string.Empty))
                    : new HypData(value ?? string.Empty);
                _property.Set(_target, data);

                Value = value ?? string.Empty;
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, $"Inspector failed to set property '{Name}': {ex.Message}");

                RefreshValue();
            }
        }

        private void CommitEnumValue(object? value)
        {
            if (!_target.IsValid || !_isEnumProperty)
            {
                return;
            }

            try
            {
                using HypData data = new HypData(value);
                _property.Set(_target, data);
                Value = FormatValue(value);
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, $"Inspector failed to set enum property '{Name}': {ex.Message}");
                RefreshValue();
            }
        }

        private void OnFlagEntryChanged()
        {
            if (_isRefreshing)
            {
                return;
            }

            CommitEnumFlagsValue();
        }

        private void UpdateFlagSelectionsFromValue(object? rawValue)
        {
            if (EnumFlagEntries == null)
            {
                return;
            }

            _isRefreshing = true;
            try
            {
                ulong currentValue = rawValue != null ? Convert.ToUInt64(rawValue) : 0ul;

                foreach (EnumFlagEntry entry in EnumFlagEntries)
                {
                    ulong flagValue = entry.Value != null ? Convert.ToUInt64(entry.Value) : 0ul;
                    entry.IsSelected = ((currentValue & flagValue) == flagValue) && flagValue != 0;
                }

                // handle zero flag explicitly
                foreach (EnumFlagEntry entry in EnumFlagEntries)
                {
                    if ((entry.Value == null || Convert.ToUInt64(entry.Value) == 0ul) && currentValue == 0ul)
                    {
                        entry.IsSelected = true;
                    }
                }
            }
            finally
            {
                _isRefreshing = false;
            }
        }

        private void CommitEnumFlagsValue()
        {
            if (!_target.IsValid || !_isEnumFlagsProperty || EnumFlagEntries == null)
            {
                return;
            }

            try
            {
                ulong combined = 0ul; // @TODO use correct underlying type -- dynamic ?
                Type? managedType = null;

                foreach (EnumFlagEntry entry in EnumFlagEntries)
                {
                    if (!entry.IsSelected || entry.Value == null)
                    {
                        continue;
                    }

                    combined |= Convert.ToUInt64(entry.Value);
                }

                // If we didn't get a type from entries, fall back to the current value type
                if (valueType == null && EnumFlagEntries.Count > 0)
                {
                    object? firstVal = EnumFlagEntries[0].Value;
                    if (firstVal != null)
                    {
                        valueType = firstVal.GetType();
                    }
                }

                using HypData data = new HypData(combined);
                _property.Set(_target, data);
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, $"Inspector failed to set enum flags property '{Name}': {ex.Message}");
                RefreshValue();
            }
        }

        private static bool IsNameType(TypeInfo typeInfo)
        {
            if (typeInfo.Class?.Name is Name typeName)
            {
                return typeName == "Name";
            }

            return false;
        }

        private static string FormatValue(object? value)
        {
            if (value == null)
            {
                return "(null)";
            }

            switch (value)
            {
                case string str:
                    return str;
                case bool boolean:
                    return boolean ? "True" : "False";
                case Enum enumValue:
                    return enumValue.ToString();
                case Name name:
                    return name.ToString();
                case byte[] bytes:
                    return $"{bytes.Length} byte(s)";
                case ObjectBase obj when obj.IsValid:
                    return obj.Class.Name.ToString();
                case ObjectBase:
                    return "(invalid object)";
                case Array array:
                    return $"{array.Length} item(s)";
                default:
                    return value.ToString() ?? value.GetType().Name;
            }
        }

        public sealed class EnumFlagEntry : ViewModelBase
        {
            private readonly Action _onChanged;
            private bool _isSelected;

            public EnumFlagEntry(string name, object? value, Action onChanged)
            {
                Name = name;
                Value = value;
                _onChanged = onChanged;
            }

            public string Name { get; }
            public object? Value { get; }

            public bool IsSelected
            {
                get => _isSelected;
                set
                {
                    if (SetProperty(ref _isSelected, value))
                    {
                        _onChanged?.Invoke();
                    }
                }
            }
        }
    }
}
