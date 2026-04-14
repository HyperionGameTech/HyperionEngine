using System;
using System.Threading;
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

        public override bool ShowInlineLabel => false;

        public bool IsChecked
        {
            get => _isChecked;
            set
            {
                if (SetProperty(ref _isChecked, value) && _isRefreshing == 0)
                {
                    CommitBoolValue(value);
                }
            }
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
                    bool boolValue = Convert.ToBoolean(boxed.GetValue() ?? false);

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        Value = boolValue ? "True" : "False";
                        _isChecked = boolValue;
                        OnPropertyChanged(nameof(IsChecked));
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
                }
            });
        }

        private void CommitBoolValue(bool value)
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            bool captured = value;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(captured);
                    SetPropertyValue(boxed);

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;
                        Value = captured ? "True" : "False";
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to write property '{_property.Name}': {ex.Message}");
                }
            });
        }
    }
}
