using System;
using System.Collections.Generic;
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

        public IList<EnumEntry> EnumEntries => _enumEntries;

        public object? SelectedEnumValue
        {
            get => _selectedEnumValue;
            set
            {
                if (SetProperty(ref _selectedEnumValue, value) && !_isRefreshing)
                {
                    CommitEnumValue(value);
                }
            }
        }

        public override bool IsEnumEditable => _enumEntries.Count > 0;

        public override void RefreshValue()
        {
            _isRefreshing = true;

            try
            {
                if (!_target.IsValid)
                {
                    Value = "(invalid target)";
                    SetProperty(ref _selectedEnumValue, null);
                    return;
                }

                using HypData data = _property.Get(_target);
                object? rawValue = data.GetValue();
                Value = FormatValue(rawValue);
                SetProperty(ref _selectedEnumValue, rawValue);
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Warn, $"Inspector failed to read property '{Name}': {ex.Message}");
                Value = "(unavailable)";
                SetProperty(ref _selectedEnumValue, null);
            }
            finally
            {
                _isRefreshing = false;
            }
        }

        private void CommitEnumValue(object? value)
        {
            if (!_target.IsValid || value == null)
            {
                return;
            }

            try
            {
                using HypData data = new HypData(value);
                _property.Set(_target, data);
                Value = FormatValue(value);
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, $"Inspector failed to set enum property '{Name}': {ex.Message}");
                RefreshValue();
            }
        }

        private void BuildEnumEntries(Class enumClass)
        {
            foreach (StaticField staticField in enumClass.StaticFields)
            {
                try
                {
                    object? enumValue = staticField.ReadObject();
                    _enumEntries.Add(new EnumEntry(staticField.Name.ToString(), enumValue));
                    Logger.Log(LogType.Debug, $"Inspector added enum static field '{staticField.Name}' to enum values for property '{Name}'");
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to read enum static field '{staticField.Name}': {ex.Message}");
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
