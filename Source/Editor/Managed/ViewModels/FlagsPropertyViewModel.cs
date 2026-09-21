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
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                List<object?> rawValues;

                try
                {
                    rawValues = ReadAllTargets(boxed => boxed.GetValue());
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
                            bool isShared = rawValues.TrueForAll(rawValue => ValuesEqual(rawValue, rawValues[0]));

                            Value = isShared ? FormatValue(rawValues[0]) : string.Empty;
                            HasMixedValues = !isShared;

                            UpdateFlagSelectionsFromValues(rawValues);
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private static void BuildEnumFlagEntryTitleAndDescription(StaticField staticField, out string outTitle, out string outDescription)
        {
            ClassAttribute? titleAttribute = staticField.GetAttribute("title");
            outTitle = titleAttribute?.GetString() ?? staticField.Name.ToString();

            ClassAttribute? descriptionAttribute = staticField.GetAttribute("description");
            outDescription = descriptionAttribute?.GetString() ?? string.Empty;
        }

        private void BuildFlagEntries(Class enumClass)
        {
            foreach (StaticField staticField in enumClass.StaticFields)
            {
                try
                {
                    // if it has editor attribute and set to false, then skip
                    ClassAttribute? attrEditor = staticField.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        continue;
                    }

                    object? flagValue = staticField.ReadObject();

                    string title;
                    string description;

                    BuildEnumFlagEntryTitleAndDescription(staticField, out title, out description);

                    EnumFlagEntry? entry = null;
                    entry = new EnumFlagEntry(title, description, flagValue, () => OnFlagEntryChanged(entry!));

                    _enumFlagEntries.Add(entry);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read enum flag static field '{staticField.Name}': {ex.Message}");
                }
            }
        }

        private void OnFlagEntryChanged(EnumFlagEntry changedEntry)
        {
            if (IsApplyingModelValue)
            {
                return;
            }

            CommitEnumFlagsValue(changedEntry);
        }

        // An entry is indeterminate (null) when the selected objects disagree on it.
        private void UpdateFlagSelectionsFromValues(List<object?> rawValues)
        {
            foreach (EnumFlagEntry entry in _enumFlagEntries)
            {
                ulong flagValue = entry.Value != null ? Convert.ToUInt64(entry.Value) : 0ul;

                bool? isSelected = null;

                for (int i = 0; i < rawValues.Count; i++)
                {
                    ulong currentValue = rawValues[i] != null ? Convert.ToUInt64(rawValues[i]) : 0ul;

                    // A zero-valued entry (e.g. None) is selected only when no flags are set.
                    bool isSet = flagValue != 0 ? (currentValue & flagValue) == flagValue : currentValue == 0ul;

                    if (i == 0)
                    {
                        isSelected = isSet;
                    }
                    else if (isSelected != isSet)
                    {
                        isSelected = null;
                        break;
                    }
                }

                entry.IsSelected = isSelected;
            }
        }

        private void CommitEnumFlagsValue(EnumFlagEntry changedEntry)
        {
            ulong combined = 0ul;

            foreach (EnumFlagEntry entry in _enumFlagEntries)
            {
                if (entry.IsSelected != true || entry.Value == null)
                {
                    continue;
                }

                combined |= Convert.ToUInt64(entry.Value);
            }

            ulong changedFlag = changedEntry.Value != null ? Convert.ToUInt64(changedEntry.Value) : 0ul;
            bool isChangedFlagSet = changedEntry.IsSelected == true;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    if (IsMultiTarget)
                    {
                        // Only the toggled flag changes; every other bit stays as each object has it.
                        CommitPropertyChange($"Set {Label}", current =>
                        {
                            object? raw = current.GetValue();
                            ulong currentValue = raw != null ? Convert.ToUInt64(raw) : 0ul;

                            return ToIntegralTypeOf(isChangedFlagSet ? currentValue | changedFlag : currentValue & ~changedFlag, raw);
                        });
                    }
                    else
                    {
                        using BoxedValue boxed = new BoxedValue(combined);
                        CommitPropertyChange($"Set {Label}", boxed);
                    }

                    Dispatcher.UIThread.Post(() => ValueCommitted?.Invoke());
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to set enum flags property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        public sealed class EnumFlagEntry : ViewModelBase
        {
            private readonly Action _onChanged;
            private bool? _isSelected;

            public EnumFlagEntry(string title, string? description, object? value, Action onChanged)
            {
                Title = title;
                Description = description ?? string.Empty;
                Value = value;
                _onChanged = onChanged;
            }

            public string Title { get; }
            public string Description { get; }

            public object? Value { get; }

            public bool? IsSelected
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
