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

        public EnumPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, Class? enumClass, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly)
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
                if (SetProperty(ref _selectedEnumValue, value) && !IsApplyingModelValue)
                {
                    CommitEnumValue(value);
                }
            }
        }

        public override bool IsEnumEditable => _enumEntries.Count > 0;

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
                    rawValue = boxed.GetValue();
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
                            Value = FormatValue(rawValue);
                            SelectedEnumValue = MatchEntryValue(rawValue);
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        // The combo box's items are the boxed values built from the enum's static fields, so the
        // selection has to be the identical boxed instance for the binding to show it as selected.
        private object? MatchEntryValue(object? rawValue)
        {
            if (rawValue == null)
            {
                return null;
            }

            ulong raw;

            try
            {
                raw = Convert.ToUInt64(rawValue);
            }
            catch
            {
                return rawValue;
            }

            foreach (EnumEntry entry in _enumEntries)
            {
                if (entry.Value == null)
                {
                    continue;
                }

                try
                {
                    if (Convert.ToUInt64(entry.Value) == raw)
                    {
                        return entry.Value;
                    }
                }
                catch
                {
                    // Skip entries we can't convert.
                }
            }

            return rawValue;
        }

        private void CommitEnumValue(object? value)
        {
            if (value == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(value);
                    CommitPropertyChange($"Set {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to set enum property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
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
