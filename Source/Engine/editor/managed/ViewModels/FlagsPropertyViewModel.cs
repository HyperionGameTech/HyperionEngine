using System;
using System.Collections.Generic;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class FlagsPropertyViewModel : InspectorPropertyViewModelBase
    {
        private readonly List<EnumFlagEntry> _enumFlagEntries = new List<EnumFlagEntry>();

        public event Action? ValueCommitted;

        public FlagsPropertyViewModel(ObjectBase target, Property property, Class? enumClass, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
            if (enumClass == null)
                throw new ArgumentNullException(nameof(enumClass));

            BuildFlagEntries(enumClass.Value);
        }

        public FlagsPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, Class? enumClass, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            if (enumClass == null)
                throw new ArgumentNullException(nameof(enumClass));

            BuildFlagEntries(enumClass.Value);
        }

        public FlagsPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, Class? enumClass, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            if (enumClass == null)
                throw new ArgumentNullException(nameof(enumClass));

            BuildFlagEntries(enumClass.Value);
        }

        public IList<EnumFlagEntry> EnumFlagEntries => _enumFlagEntries;

        public override bool IsEnumFlagsEditable => _enumFlagEntries.Count > 0;

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
                        try
                        {
                            Value = FormatValue(rawValue);

                            UpdateFlagSelectionsFromValue(rawValue);
                        }
                        finally
                        {
                            _isRefreshing = 0;
                        }
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
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
                    Logger.Log(LogLevel.Debug, $"Inspector added enum flag static field '{staticField.Name}' to enum flag values for property '{_property.Name}'");
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read enum flag static field '{staticField.Name}': {ex.Message}");
                }
            }
        }

        private void OnFlagEntryChanged()
        {
            if (_isRefreshing == 1)
            {
                return;
            }

            CommitEnumFlagsValue();
        }

        private void UpdateFlagSelectionsFromValue(object? rawValue)
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

        private void CommitEnumFlagsValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    ulong combined = 0ul;

                    foreach (EnumFlagEntry entry in _enumFlagEntries)
                    {
                        if (!entry.IsSelected || entry.Value == null)
                        {
                            continue;
                        }

                        combined |= Convert.ToUInt64(entry.Value);
                    }

                    using BoxedValue boxed = new BoxedValue(combined);
                    CommitPropertyChange($"Set {Label}", boxed);

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        ValueCommitted?.Invoke();
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Error, $"Inspector failed to set enum flags property '{_property.Name}': {ex.Message}");

                    RefreshValue();
                }
            });
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
