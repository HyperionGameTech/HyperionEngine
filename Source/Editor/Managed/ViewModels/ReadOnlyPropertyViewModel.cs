using System;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ReadOnlyPropertyViewModel : InspectorPropertyViewModelBase
    {
        public ReadOnlyPropertyViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
        }

        public ReadOnlyPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
        }

        public ReadOnlyPropertyViewModel(string label, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, default, getter, setter, isReadOnly)
        {
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

                        Value = FormatValue(rawValue);
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
                }
            });
        }
    }
}
