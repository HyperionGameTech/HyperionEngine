using System;
using System.Collections.Generic;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class FlagsPropertyViewModel : InspectorPropertyViewModelBase
    {
        private readonly List<EnumFlagEntry> _enumFlagEntries = new List<EnumFlagEntry>();

        public FlagsPropertyViewModel(ObjectBase target, Property property, Class? enumClass, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
            if (enumClass == null)
                throw new ArgumentNullException(nameof(enumClass));

            BuildFlagEntries(enumClass.Value);
        }

        public IList<EnumFlagEntry> EnumFlagEntries => _enumFlagEntries;

        public override bool IsEnumFlagsEditable => _enumFlagEntries.Count > 0;

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
                        UpdateFlagSelectionsFromValue(rawValue);
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");

                    _isRefreshing = false;
                }
            });
        }

        private void BuildFlagEntries(Class enumClass)
        {
            foreach (StaticField staticField in enumClass.StaticFields)
            {
                try
                {
                    object? flagValue = staticField.ReadObject();
                    _enumFlagEntries.Add(new EnumFlagEntry(staticField.Name.ToString(), flagValue, OnFlagEntryChanged));
                    Logger.Log(LogType.Debug, $"Inspector added enum flag static field '{staticField.Name}' to enum flag values for property '{Name}'");
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to read enum flag static field '{staticField.Name}': {ex.Message}");
                }
            }
        }

        private void OnFlagEntryChanged()
        {
            if (_isRefreshing)
            {
                return;
            }

            CommitEnumFlagsValue();
        }

        private void UpdateFlagSelectionsFromValue(object? rawValue)
        {
            _isRefreshing = true;
            try
            {
                ulong currentValue = rawValue != null ? Convert.ToUInt64(rawValue) : 0ul;

                foreach (EnumFlagEntry entry in _enumFlagEntries)
                {
                    ulong flagValue = entry.Value != null ? Convert.ToUInt64(entry.Value) : 0ul;
                    entry.IsSelected = ((currentValue & flagValue) == flagValue) && flagValue != 0;
                }

                foreach (EnumFlagEntry entry in _enumFlagEntries)
                {
                    if ((entry.Value == null || Convert.ToUInt64(entry.Value) == 0ul) && currentValue == 0ul)
                    {
                        entry.IsSelected = true;
                    }
                }
            }
            finally
            {
                _isRefreshing = false;
            }
        }

        private void CommitEnumFlagsValue()
        {
            if (!_target.IsValid || _enumFlagEntries.Count == 0)
            {
                return;
            }

            try
            {
                ulong combined = 0ul;
                Type? valueType = null;

                foreach (EnumFlagEntry entry in _enumFlagEntries)
                {
                    if (!entry.IsSelected || entry.Value == null)
                    {
                        continue;
                    }

                    valueType ??= entry.Value.GetType();
                    combined |= Convert.ToUInt64(entry.Value);
                }

                if (valueType == null)
                {
                    return;
                }

                // @TODO : Not valid to use Enum.ToObject for this
                object finalValue = Enum.ToObject(valueType, combined);

                using HypData data = new HypData(finalValue);
                _property.Set(_target, data);

                Value = FormatValue(finalValue);
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, $"Inspector failed to set enum flags property '{Name}': {ex.Message}");
                RefreshValue();
            }
        }

        public sealed class EnumFlagEntry : ViewModelBase
        {
            private readonly Action _onChanged;
            private bool _isSelected;

            public EnumFlagEntry(string name, object? value, Action onChanged)
            {
                Name = name;
                Value = value;
                _onChanged = onChanged;
            }

            public string Name { get; }

            public object? Value { get; }

            public bool IsSelected
            {
                get => _isSelected;
                set
                {
                    if (SetProperty(ref _isSelected, value))
                    {
                        _onChanged?.Invoke();
                    }
                }
            }
        }
    }
}
