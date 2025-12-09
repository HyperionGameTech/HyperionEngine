using System;
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
            _isRefreshing = true;

            try
            {
                if (!_target.IsValid)
                {
                    Value = "(invalid target)";
                    return;
                }

                using HypData data = _property.Get(_target);
                object? rawValue = data.GetValue();
                Value = FormatValue(rawValue);
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");
                Value = "(unavailable)";
            }
            finally
            {
                _isRefreshing = false;
            }
        }
    }
}
