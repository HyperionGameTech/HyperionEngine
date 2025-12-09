using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class TextPropertyViewModel : InspectorPropertyViewModelBase
    {
        private readonly bool _isNameProperty;
        private string _editableValue = string.Empty;

        public TextPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, bool isNameProperty)
            : base(target, property, isReadOnly)
        {
            _isNameProperty = isNameProperty;
        }

        public override bool IsTextEditable => true;

        public bool IsStringEditable => !_isNameProperty;

        public bool IsNameEditable => _isNameProperty;

        public string EditableValue
        {
            get => _editableValue;
            set
            {
                if (SetProperty(ref _editableValue, value) && !_isRefreshing)
                {
                    CommitEditableText(value);
                }
            }
        }

        public override void RefreshValue()
        {
            _isRefreshing = true;

            try
            {
                if (!_target.IsValid)
                {
                    Value = "(invalid target)";
                    EditableValue = string.Empty;
                    return;
                }

                using HypData data = _property.Get(_target);
                object? rawValue = data.GetValue();
                Value = FormatValue(rawValue);
                EditableValue = rawValue?.ToString() ?? string.Empty;
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");
                Value = "(unavailable)";
                EditableValue = string.Empty;
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
    }
}
