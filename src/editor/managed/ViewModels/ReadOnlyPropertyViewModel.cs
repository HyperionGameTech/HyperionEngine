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

        public override void RefreshValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToGameThread(() =>
            {
                try
                {
                    using BoxedValue boxed = _property.Get(_target);
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

                    Logger.Log(LogType.Warn, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
                }
            });
        }
    }
}
