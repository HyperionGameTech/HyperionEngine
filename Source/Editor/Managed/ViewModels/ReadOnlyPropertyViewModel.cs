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
                    object? raw = boxed.GetValue();

                    rawValue = raw is Hyperion.UUID uuid ? uuid.ToString() : raw;
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
                        ApplyModelValue(() => Value = FormatValue(rawValue));
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }
    }
}
