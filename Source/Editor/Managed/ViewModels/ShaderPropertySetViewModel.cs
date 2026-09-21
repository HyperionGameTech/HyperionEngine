using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ShaderPropertySetViewModel : InspectorPropertyViewModelBase
    {
        private const string ShaderNamePropertyName = "ShaderName";

        private readonly Property? _shaderNameProperty;

        public ObservableCollection<ShaderPropertyGroupViewModel> Groups { get; } = new();

        public bool HasGroups => Groups.Count > 0;

        private string _statusText = string.Empty;
        public string StatusText
        {
            get => _statusText;
            private set
            {
                if (SetProperty(ref _statusText, value))
                {
                    OnPropertyChanged(nameof(HasStatusText));
                }
            }
        }

        public bool HasStatusText => StatusText.Length != 0;

        private bool _isExpanded = true;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public override bool ShowInlineLabel => false;
        public override bool SupportsMultipleTargets => false;
        public override bool ShowsDescriptionInOwnTemplate => true;

        public ShaderPropertySetViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _shaderNameProperty = FindSiblingProperty(new Name(ShaderNamePropertyName));
        }

        public ShaderPropertySetViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _shaderNameProperty = FindSiblingProperty(new Name(ShaderNamePropertyName));
        }

        public ShaderPropertySetViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            // No object context in this mode, so there is no sibling ShaderName to narrow the
            // option list down to - only the properties already set can be shown.
            _shaderNameProperty = null;
        }

        /// <summary>Sim thread. Reads the set out of the property, via the pointer since it is a plain bitset.</summary>
        private unsafe ShaderPropertySet ReadPropertySet()
        {
            using BoxedValue boxed = GetPropertyValue();

            if (boxed.IsNull || boxed.Pointer == IntPtr.Zero)
            {
                return default;
            }

            return *(ShaderPropertySet*)boxed.Pointer;
        }

        private Name ReadShaderName()
        {
            if (_shaderNameProperty == null)
            {
                return Name.Invalid;
            }

            try
            {
                using BoxedValue boxed = GetSiblingPropertyValue(_shaderNameProperty.Value);

                return boxed.IsNull ? Name.Invalid : boxed.Buffer.ReadName();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"ShaderPropertySetViewModel: failed to read '{ShaderNamePropertyName}': {ex.Message}");

                return Name.Invalid;
            }
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                ShaderPropertySet propertySet;
                Name shaderName;
                IReadOnlyList<ShaderPropertyOption> shaderOptions;

                try
                {
                    propertySet = ReadPropertySet();
                    shaderName = ReadShaderName();
                    shaderOptions = shaderName.Valid
                        ? ShaderProperties.GetShaderOptions(shaderName)
                        : Array.Empty<ShaderPropertyOption>();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read shader property set '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                // Bits that no option accounts for are described individually so they can be cleared.
                var setIds = propertySet.GetSetPropertyIds();
                var knownIds = new HashSet<uint>(shaderOptions.Select(option => option.PropertyId));
                var foreignOptions = new List<ShaderPropertyOption>();

                foreach (uint propertyId in setIds)
                {
                    if (knownIds.Contains(propertyId))
                    {
                        continue;
                    }

                    foreignOptions.Add(ShaderProperties.GetPropertyById(propertyId)
                        ?? new ShaderPropertyOption(Name.Invalid, string.Empty, propertyId, ShaderPropertyOptionKind.Toggle));
                }

                ShaderPropertySet capturedSet = propertySet;
                Name capturedShaderName = shaderName;
                IReadOnlyList<ShaderPropertyOption> capturedShaderOptions = shaderOptions;
                List<ShaderPropertyOption> capturedForeignOptions = foreignOptions;

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            RebuildGroups(capturedShaderOptions, capturedForeignOptions);

                            foreach (ShaderPropertyGroupViewModel group in Groups)
                            {
                                group.ApplySelectionFrom(capturedSet);
                            }

                            UpdateSummary(capturedShaderName, capturedShaderOptions.Count);
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private void RebuildGroups(IReadOnlyList<ShaderPropertyOption> shaderOptions, List<ShaderPropertyOption> foreignOptions)
        {
            // The group list only changes when the shader changes or a foreign bit appears, so keep
            // the existing view models when it would come out the same - rebuilding on every refresh
            // would close whatever combo box the user has open.
            var signature = shaderOptions.Select(option => option.PropertyId)
                .Concat(foreignOptions.Select(option => option.PropertyId))
                .ToList();

            if (GroupsMatch(signature))
            {
                return;
            }

            Groups.Clear();

            foreach (IGrouping<ulong, ShaderPropertyOption> grouping in shaderOptions.GroupBy(option => option.Name.HashCode))
            {
                List<ShaderPropertyOption> options = grouping
                    .Where(option => option.IsStorable)
                    .OrderBy(option => option.ValueString, StringComparer.OrdinalIgnoreCase)
                    .ToList();

                if (options.Count == 0)
                {
                    continue;
                }

                Groups.Add(new ShaderPropertyGroupViewModel(options[0].Name.ToString(), options, isDeclaredByShader: true, !_isReadOnly, OnGroupSelectionChanged));
            }

            foreach (ShaderPropertyOption option in foreignOptions)
            {
                string name = option.Name.Valid ? option.Name.ToString() : $"Unknown #{option.PropertyId}";

                Groups.Add(new ShaderPropertyGroupViewModel(name, new List<ShaderPropertyOption> { option }, isDeclaredByShader: false, !_isReadOnly, OnGroupSelectionChanged));
            }

            OnPropertyChanged(nameof(HasGroups));
        }

        private bool GroupsMatch(List<uint> propertyIds)
        {
            var existing = Groups.SelectMany(group => group.Options).Select(option => option.PropertyId).ToList();

            return existing.Count == propertyIds.Count && !existing.Except(propertyIds).Any();
        }

        private void UpdateSummary(Name shaderName, int optionCount)
        {
            string summary = string.Join(", ", Groups.SelectMany(group => group.GetSelectedDescriptions()));

            Value = summary.Length != 0 ? summary : "None";

            if (!shaderName.Valid)
            {
                StatusText = "No shader is set, so only properties already applied are listed.";
            }
            else if (optionCount == 0)
            {
                StatusText = $"'{shaderName}' has no compiled variants to read permutations from.";
            }
            else
            {
                StatusText = string.Empty;
            }
        }

        /// <summary>
        /// UI thread. Rebuilds the whole set from the group selections and commits it. The set is a
        /// single value, so a group's change is applied on top of a freshly read set rather than on
        /// top of whatever was read when the panel was built.
        /// </summary>
        private void OnGroupSelectionChanged()
        {
            if (IsApplyingModelValue || _isReadOnly)
            {
                return;
            }

            var enabledIds = Groups.SelectMany(group => group.GetSelectedPropertyIds()).ToList();
            var candidateIds = Groups.SelectMany(group => group.Options).Select(option => option.PropertyId).ToList();

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    PreWriteCallback?.Invoke();

                    ShaderPropertySet propertySet = ReadPropertySet();

                    foreach (uint propertyId in candidateIds)
                    {
                        propertySet.Set(propertyId, enabledIds.Contains(propertyId));
                    }

                    using BoxedValue boxed = new BoxedValue(propertySet);
                    CommitPropertyChange($"Set {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to set shader property set '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }
    }

    /// <summary>
    /// The options sharing one shader property name: a single toggle for a valueless permutation,
    /// or a pick-one list for a value group.
    /// </summary>
    public sealed class ShaderPropertyGroupViewModel : ViewModelBase
    {
        private const string NoValueLabel = "(none)";

        private readonly Action _onChanged;
        private bool _isChecked;
        private int _selectedValueIndex;

        public ShaderPropertyGroupViewModel(string name, List<ShaderPropertyOption> options, bool isDeclaredByShader, bool isEditable, Action onChanged)
        {
            Options = options;
            IsDeclaredByShader = isDeclaredByShader;
            IsEditable = isEditable;
            _onChanged = onChanged;

            Name = name;
            IsToggle = options.Count == 1 && options[0].Kind == ShaderPropertyOptionKind.Toggle;

            ValueLabels = IsToggle
                ? Array.Empty<string>()
                : new[] { NoValueLabel }.Concat(options.Select(option => option.ValueString)).ToArray();
        }

        public List<ShaderPropertyOption> Options { get; }

        public string Name { get; }

        public bool IsToggle { get; }

        public bool IsValueGroup => !IsToggle;

        public bool IsDeclaredByShader { get; }

        public bool IsEditable { get; }

        public IReadOnlyList<string> ValueLabels { get; }

        public string? Tooltip => IsDeclaredByShader
            ? null
            : "Not declared by this shader - it was left over from another one and can only be cleared.";

        public double Opacity => IsDeclaredByShader ? 1.0 : 0.6;

        public bool IsChecked
        {
            get => _isChecked;
            set
            {
                if (SetProperty(ref _isChecked, value))
                {
                    _onChanged();
                }
            }
        }

        /// <summary>Index into <see cref="ValueLabels"/>: 0 means no value of this group is applied.</summary>
        public int SelectedValueIndex
        {
            get => _selectedValueIndex;
            set
            {
                // The ComboBox reports -1 while its item list is being swapped out; that is not a
                // user edit and committing it would clear the group.
                if (value < 0)
                {
                    return;
                }

                if (SetProperty(ref _selectedValueIndex, value))
                {
                    _onChanged();
                }
            }
        }

        public void ApplySelectionFrom(ShaderPropertySet propertySet)
        {
            if (IsToggle)
            {
                _isChecked = propertySet.Test(Options[0].PropertyId);
                OnPropertyChanged(nameof(IsChecked));

                return;
            }

            _selectedValueIndex = 0;

            for (int i = 0; i < Options.Count; i++)
            {
                if (propertySet.Test(Options[i].PropertyId))
                {
                    _selectedValueIndex = i + 1;
                    break;
                }
            }

            OnPropertyChanged(nameof(SelectedValueIndex));
        }

        public IEnumerable<uint> GetSelectedPropertyIds()
        {
            if (IsToggle)
            {
                if (_isChecked)
                {
                    yield return Options[0].PropertyId;
                }

                yield break;
            }

            if (_selectedValueIndex > 0 && _selectedValueIndex <= Options.Count)
            {
                yield return Options[_selectedValueIndex - 1].PropertyId;
            }
        }

        public IEnumerable<string> GetSelectedDescriptions()
        {
            if (IsToggle)
            {
                if (_isChecked)
                {
                    yield return Name;
                }

                yield break;
            }

            if (_selectedValueIndex > 0 && _selectedValueIndex <= Options.Count)
            {
                yield return $"{Name}={Options[_selectedValueIndex - 1].ValueString}";
            }
        }
    }
}
