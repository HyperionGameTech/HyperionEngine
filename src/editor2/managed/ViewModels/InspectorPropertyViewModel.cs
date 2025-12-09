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

        private readonly Class? _class;

        private string _value = string.Empty;
        private string _editableValue = string.Empty;
        private Array? _enumValues;
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
        public bool IsEnumEditable => _isEnumProperty && EnumValues != null && EnumValues.Length > 0;
        public bool ShowTextValue => !IsTextEditable && !IsEnumEditable;

        public Array? EnumValues
        {
            get => _enumValues;
            private set => SetProperty(ref _enumValues, value);
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
                Logger.Log(LogType.Debug, $" - IsEnumType: {typeInfoClass.IsEnumType}");
                Logger.Log(LogType.Debug, $" - IsNameType: {IsNameType(typeInfo)}");
                
                if (typeInfoClass.IsEnumType)
                {
                    _isEnumProperty = true;

                    // use StaticFields of the enum class to populate EnumValues
                    List<object?> enumValuesList = new List<object?>();

                    foreach (StaticField staticField in typeInfoClass.StaticFields)
                    {
                        try
                        {
                            enumValuesList.Add(staticField.ReadObject());
                            Logger.Log(LogType.Debug, $"Inspector added enum static field '{staticField.Name}' to enum values for property '{Name}'");
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(LogType.Warn, $"Inspector failed to read enum static field '{staticField.Name}': {ex.Message}");
                        }
                    }

                    EnumValues = enumValuesList.ToArray();
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

                if (_isEnumProperty)
                {
                    // @TODO
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

            // @TODO
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
    }
}
