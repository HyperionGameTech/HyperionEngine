using System;
using Avalonia.Threading;
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
            if (_isRefreshing)
            {
                return;
            }

            _isRefreshing = true;

            _ = EngineManager.PostToGameThread(() =>
            {
                try
                {
                    using HypData data = _property.Get(_target);
                    object? rawValue = data.GetValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = false;

                        Value = FormatValue(rawValue);
                        EditableValue = rawValue?.ToString() ?? string.Empty;
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");

                    _isRefreshing = false;
                }
            });
        }

        private void CommitEditableText(string value)
        {
            if (_isRefreshing)
            {
                return;
            }

            _isRefreshing = true;

            _ = EngineManager.PostToGameThread(() =>
            {
                try
                {
                    using HypData data = _isNameProperty
                        ? new HypData(new Name(value ?? string.Empty))
                        : new HypData(value ?? string.Empty);

                    _property.Set(_target, data);

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = false;

                        Value = value ?? string.Empty;
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to write property '{Name}': {ex.Message}");

                    _isRefreshing = false;
                }
            });
        }
    }
}
