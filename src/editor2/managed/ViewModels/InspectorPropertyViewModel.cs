using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorPropertyViewModel : ViewModelBase
    {
        private readonly ObjectBase _target;
        private readonly Property _property;
        private readonly bool _isStringProperty;

        private string _value = string.Empty;
        private string _editableValue = string.Empty;
        private bool _isRefreshing;

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
                if (SetProperty(ref _editableValue, value) && !_isRefreshing && _isStringProperty)
                {
                    CommitEditableString(value);
                }
            }
        }

        public bool IsStringEditable => _isStringProperty;
        public bool ShowTextValue => !_isStringProperty;

        public InspectorPropertyViewModel(ObjectBase target, Property property)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _property = property;
            _isStringProperty = property.TypeInfo.IsString;

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
                    if (_isStringProperty)
                    {
                        EditableValue = string.Empty;
                    }

                    return;
                }

                using HypData data = _property.Get(_target);
                object? rawValue = data.GetValue();
                string formattedValue = FormatValue(rawValue);

                Value = formattedValue;

                if (_isStringProperty)
                {
                    EditableValue = rawValue as string ?? string.Empty;
                }
                else
                {
                    EditableValue = formattedValue;
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");
                Value = "(unavailable)";

                if (_isStringProperty)
                {
                    EditableValue = string.Empty;
                }
            }
            finally
            {
                _isRefreshing = false;
            }
        }

        private void CommitEditableString(string value)
        {
            if (!_target.IsValid)
            {
                return;
            }

            try
            {
                using HypData data = new HypData(value);
                _property.Set(_target, data);

                Value = value;
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, $"Inspector failed to set property '{Name}': {ex.Message}");

                RefreshValue();
            }
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
