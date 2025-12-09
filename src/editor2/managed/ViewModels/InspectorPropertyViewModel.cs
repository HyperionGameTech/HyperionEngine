using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorPropertyViewModel : ViewModelBase
    {
        private readonly ObjectBase _target;
        private readonly Property _property;

        private string _value = string.Empty;

        public Property Property => _property;

        public string Name => _property.Name.ToString();

        public string Value
        {
            get => _value;
            private set => SetProperty(ref _value, value);
        }

        public InspectorPropertyViewModel(ObjectBase target, Property property)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _property = property;

            RefreshValue();
        }

        public void RefreshValue()
        {
            if (!_target.IsValid)
            {
                Value = "(invalid target)";
                return;
            }

            try
            {
                using HypData data = _property.Get(_target);
                Value = FormatValue(data.GetValue());
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");
                Value = "(unavailable)";
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
                case ObjectBase obj when obj.IsValid:
                    return obj.Class.Name.ToString();
                case ObjectBase:
                    return "(invalid object)";
                case Array array:
                    return $"Array ({array.Length})";
                default:
                    return $"<{value.GetType().Name}>";
            }
        }
    }
}
