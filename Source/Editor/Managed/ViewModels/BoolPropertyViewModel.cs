using System;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class BoolPropertyViewModel : InspectorPropertyViewModelBase
    {
        private bool _isChecked;

        public BoolPropertyViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
        }

        public BoolPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
        }

        public BoolPropertyViewModel(string label, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, default, getter, setter, isReadOnly)
        {
        }

        public override bool ShowInlineLabel => false;

        public bool IsChecked
        {
            get => _isChecked;
            set
            {
                if (SetProperty(ref _isChecked, value) && !IsApplyingModelValue)
                {
                    CommitBoolValue(value);
                }
            }
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                bool boolValue;

                try
                {
                    using BoxedValue boxed = GetPropertyValue();
                    boolValue = Convert.ToBoolean(boxed.GetValue() ?? false);
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
                            Value = boolValue ? "True" : "False";
                            IsChecked = boolValue;
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private void CommitBoolValue(bool value)
        {
            bool captured = value;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(captured);
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
