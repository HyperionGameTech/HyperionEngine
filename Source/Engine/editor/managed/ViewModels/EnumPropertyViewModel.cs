using System;
using System.Collections.Generic;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class EnumPropertyViewModel : InspectorPropertyViewModelBase
    {
        private readonly List<EnumEntry> _enumEntries = new List<EnumEntry>();
        private object? _selectedEnumValue;

        public EnumPropertyViewModel(ObjectBase target, Property property, Class? enumClass, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
            if (enumClass == null)
                throw new ArgumentNullException(nameof(enumClass));

            BuildEnumEntries(enumClass.Value);
        }

        public EnumPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, Class? enumClass, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            if (enumClass == null)
                throw new ArgumentNullException(nameof(enumClass));

            BuildEnumEntries(enumClass.Value);
        }

        public IList<EnumEntry> EnumEntries => _enumEntries;

        public object? SelectedEnumValue
        {
            get => _selectedEnumValue;
            set
            {
                if (SetProperty(ref _selectedEnumValue, value) && _isRefreshing == 0)
                {
                    CommitEnumValue(value);
                }
            }
        }

        public override bool IsEnumEditable => _enumEntries.Count > 0;

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
                        SetProperty(ref _selectedEnumValue, rawValue);
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
                }
            });
        }

        private void CommitEnumValue(object? value)
        {
            if (value == null)
            {
                return;
            }

            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(value);
                    SetPropertyValue(boxed);

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        Value = FormatValue(value);
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Error, $"Inspector failed to set enum property '{_property.Name}': {ex.Message}");

                    RefreshValue();
                }
            });
        }

        private void BuildEnumEntries(Class enumClass)
        {
            foreach (StaticField staticField in enumClass.StaticFields)
            {
                try
                {
                    object? enumValue = staticField.ReadObject();
                    _enumEntries.Add(new EnumEntry(staticField.Name.ToString(), enumValue));
                    Logger.Log(LogLevel.Debug, $"Inspector added enum static field '{staticField.Name}' to enum values for property '{_property.Name}'");
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read enum static field '{staticField.Name}': {ex.Message}");
                }
            }
        }

        public sealed class EnumEntry
        {
            public EnumEntry(string name, object? value)
            {
                Name = name;
                Value = value;
            }

            public string Name { get; }

            public object? Value { get; }
        }
    }
}
