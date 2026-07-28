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

        public TextPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, bool isNameProperty)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _isNameProperty = isNameProperty;
        }

        public TextPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, bool isNameProperty)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _isNameProperty = isNameProperty;
        }

        public override bool IsTextEditable => true;

        public bool IsStringEditable => !_isNameProperty;

        public bool IsNameEditable => _isNameProperty;

        public string EditableValue
        {
            get => _editableValue;
            set => SetProperty(ref _editableValue, value);
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                object? rawValue;

                try
                {
                    using BoxedValue boxed = GetPropertyValue();
                    rawValue = boxed.GetValue();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            Value = FormatValue(rawValue);

                            // Don't stomp on text the user is part-way through typing.
                            if (!IsEditing)
                            {
                                EditableValue = rawValue?.ToString() ?? string.Empty;
                            }
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        public override void CommitValue()
        {
            CommitEditableText(_editableValue);
        }

        private void CommitEditableText(string value)
        {
            string captured = value ?? string.Empty;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = _isNameProperty
                        ? new BoxedValue(new Name(captured))
                        : new BoxedValue(captured);

                    CommitPropertyChange($"Set {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to write property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }
    }
}
