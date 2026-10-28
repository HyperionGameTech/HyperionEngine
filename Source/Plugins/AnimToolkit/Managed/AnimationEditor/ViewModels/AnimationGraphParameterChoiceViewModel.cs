using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class AnimationGraphParameterChoiceViewModel : TextPropertyViewModel
    {
        public const string NoneChoice = "(none)";

        private IReadOnlyList<string> _choices = new[] { NoneChoice };
        private string _modelValue = string.Empty;
        private bool _hasModelValue;
        private bool _isApplying;

        private readonly Func<IEnumerable<string>> _source;

        public AnimationGraphParameterChoiceViewModel(ObjectBase target, Property property, bool isReadOnly, Func<IEnumerable<string>> source)
            : base(target, property, isReadOnly, true)
        {
            _source = source;

            PropertyChanged += OnOwnPropertyChanged;

            RebuildChoices();
        }

        public IReadOnlyList<string> Choices
        {
            get => _choices;
            private set => SetProperty(ref _choices, value);
        }

        public string SelectedChoice
        {
            get => _modelValue.Length > 0 ? _modelValue : NoneChoice;
            set
            {
                if (_isApplying || !_hasModelValue || value == null || !_choices.Contains(value))
                {
                    return;
                }

                string name = value == NoneChoice ? string.Empty : value;

                if (name == _modelValue)
                {
                    return;
                }

                _modelValue = name;
                EditableValue = name;
                CommitValue();
            }
        }

        private void RebuildChoices()
        {
            List<string> choices = new List<string> { NoneChoice };

            choices.AddRange(_source());

            if (_modelValue.Length > 0 && !choices.Contains(_modelValue))
            {
                choices.Add(_modelValue);
            }

            if (!choices.SequenceEqual(_choices))
            {
                Choices = choices;
            }
        }

        private void OnOwnPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName != nameof(EditableValue))
            {
                return;
            }

            _isApplying = true;

            _modelValue = EditableValue;
            _hasModelValue = true;

            RebuildChoices();
            OnPropertyChanged(nameof(SelectedChoice));

            _isApplying = false;
        }
    }
}
