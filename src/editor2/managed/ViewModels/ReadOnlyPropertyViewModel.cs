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
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");
                    _isRefreshing = false;
                }
            });
        }
    }
}
