using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public sealed class ArborControlSnapshot
    {
        public uint Index;
        public string Section = string.Empty;
        public string Group = string.Empty;
        public string Label = string.Empty;
        public string Help = string.Empty;
        public ArborControlKind Kind;
        public float Min;
        public float Max = 1.0f;
        public bool Log;
        public uint CountMin;
        public uint CountMax;
        public string[] Options = Array.Empty<string>();

        public bool IsRange;
        public float Lo;
        public float Hi;
        public float Landed;
        public uint Integer;
    }

    public sealed class ArborSnapshot
    {
        public bool HasTarget;
        public string TargetName = string.Empty;
        public ArborPlantKind Kind;
        public string RecipeName = string.Empty;
        public string RecipePreset = string.Empty;
        public string[] Presets = Array.Empty<string>();
        public uint Seed;
        public uint LayoutVersion;
        public bool HasLayout;
        public ArborControlSnapshot[] Controls = Array.Empty<ArborControlSnapshot>();
        public bool IsGenerating;
        public bool IsNewPlant;
        public uint VariantCount = 1;
        public bool CanSetVariantCount;
        public bool CanBake;
        public string StatusText = string.Empty;

        internal static ArborSnapshot Read(EditorArborState state, uint? knownLayoutVersion)
        {
            ArborSnapshot snapshot = new();

            try
            {
                snapshot.HasTarget = state.HasTarget();
                snapshot.StatusText = state.GetStatusText() ?? string.Empty;

                if (!snapshot.HasTarget)
                {
                    return snapshot;
                }

                snapshot.TargetName = state.GetTargetName() ?? string.Empty;
                snapshot.Kind = (ArborPlantKind)state.GetTargetKind();
                snapshot.RecipeName = state.GetRecipeName() ?? string.Empty;
                snapshot.RecipePreset = state.GetRecipePreset() ?? string.Empty;
                snapshot.Seed = state.GetSeed();
                snapshot.LayoutVersion = state.GetLayoutVersion();
                snapshot.HasLayout = knownLayoutVersion != snapshot.LayoutVersion;
                snapshot.IsGenerating = state.IsGenerating();
                snapshot.IsNewPlant = state.IsNewPlant();
                snapshot.VariantCount = state.GetVariantCount();
                snapshot.CanSetVariantCount = state.CanSetVariantCount();
                snapshot.CanBake = state.CanBake();

                uint presetCount = state.GetPresetCount((uint)snapshot.Kind);
                snapshot.Presets = Enumerable.Range(0, (int)presetCount)
                    .Select(i => state.GetPresetName((uint)snapshot.Kind, (uint)i) ?? string.Empty)
                    .ToArray();

                uint count = state.GetControlCount();
                snapshot.Controls = new ArborControlSnapshot[count];

                for (uint index = 0; index < count; index++)
                {
                    ArborControlSnapshot control = new ArborControlSnapshot { Index = index };
                    control.Kind = (ArborControlKind)state.GetControlKind(index);

                    if (snapshot.HasLayout)
                    {
                        control.Section = state.GetControlSection(index) ?? string.Empty;
                        control.Group = state.GetControlGroup(index) ?? string.Empty;
                        control.Label = state.GetControlLabel(index) ?? string.Empty;
                        control.Help = state.GetControlHelp(index) ?? string.Empty;
                        control.Min = state.GetControlMin(index);
                        control.Max = state.GetControlMax(index);
                        control.Log = state.IsControlLog(index);
                        control.CountMin = state.GetControlCountMin(index);
                        control.CountMax = state.GetControlCountMax(index);
                        control.Options = Enumerable.Range(0, (int)state.GetControlOptionCount(index))
                            .Select(o => state.GetControlOption(index, (uint)o) ?? string.Empty)
                            .ToArray();
                    }

                    if (control.Kind == ArborControlKind.Ranged)
                    {
                        control.IsRange = state.IsControlRange(index);
                        control.Lo = state.GetControlLo(index);
                        control.Hi = state.GetControlHi(index);
                        control.Landed = state.GetControlLanded(index);
                    }
                    else
                    {
                        control.Integer = state.GetControlInteger(index);
                    }

                    snapshot.Controls[index] = control;
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Failed to read Arbor state: {ex.Message}");
            }

            return snapshot;
        }
    }

    public sealed class ArborControlRowViewModel : ViewModelBase
    {
        private readonly ArborPanelViewModel _panel;
        private ArborControlSnapshot _control;

        private DateTime _editedAt = DateTime.MinValue;

        public uint Index => _control.Index;
        public string Label => _control.Label;
        public string Help => _control.Help;

        public bool IsRanged => _control.Kind == ArborControlKind.Ranged;
        public bool IsSingle => IsRanged && !_control.IsRange;
        public bool IsCount => _control.Kind == ArborControlKind.Count;
        public bool IsChoice => _control.Kind == ArborControlKind.Choice;
        public bool IsToggle => _control.Kind == ArborControlKind.Toggle;

        public double CountMin => _control.CountMin;
        public double CountMax => _control.CountMax;
        public string[] Options => _control.Options;

        public ArborControlRowViewModel(ArborPanelViewModel panel, ArborControlSnapshot control)
        {
            _panel = panel;
            _control = control;
        }

        public bool IsRange
        {
            get => _control.IsRange;
            set
            {
                if (_control.IsRange == value)
                {
                    return;
                }

                if (value)
                {
                    _control.Hi = _control.Lo;
                }
                else
                {
                    _control.Lo = _control.Landed;
                    _control.Hi = _control.Landed;
                }

                _control.IsRange = value;
                PushRanged();
                OnPropertyChanged(nameof(IsSingle));
                RaiseValues();
            }
        }

        public double LoPosition
        {
            get => ToPosition(_control.Lo);
            set
            {
                _control.Lo = FromPosition(value);
                PushRanged();
                RaiseValues();
            }
        }

        public double HiPosition
        {
            get => ToPosition(_control.Hi);
            set
            {
                _control.Hi = FromPosition(value);
                PushRanged();
                RaiseValues();
            }
        }

        public string LoText
        {
            get => Short(_control.Lo);
            set
            {
                if (TryParse(value, out float lo) && lo != _control.Lo)
                {
                    _control.Lo = lo;
                    PushRanged();
                    RaiseValues();
                }
            }
        }

        public string HiText
        {
            get => Short(_control.Hi);
            set
            {
                if (TryParse(value, out float hi) && hi != _control.Hi)
                {
                    _control.Hi = hi;
                    PushRanged();
                    RaiseValues();
                }
            }
        }

        public string LandedText => Short(_control.Landed);
        public string LandedTip => $"Where this seed lands in the range: {Short(_control.Landed)}";

        public double CountValue
        {
            get => _control.Integer;
            set
            {
                uint integer = (uint)Math.Max(0, Math.Round(value));

                if (integer != _control.Integer)
                {
                    _control.Integer = integer;
                    PushInteger();
                    OnPropertyChanged(nameof(CountText));
                }
            }
        }

        public string CountText
        {
            get => _control.Integer.ToString(CultureInfo.InvariantCulture);
            set
            {
                if (uint.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint integer))
                {
                    CountValue = integer;
                    OnPropertyChanged(nameof(CountValue));
                }
            }
        }

        public int SelectedOption
        {
            get => (int)_control.Integer;
            set
            {
                if (value >= 0 && (uint)value != _control.Integer)
                {
                    _control.Integer = (uint)value;
                    PushInteger();
                }
            }
        }

        public bool IsOn
        {
            get => _control.Integer != 0;
            set
            {
                if (value != (_control.Integer != 0))
                {
                    _control.Integer = value ? 1u : 0u;
                    PushInteger();
                }
            }
        }

        internal void Update(ArborControlSnapshot values)
        {
            if ((DateTime.UtcNow - _editedAt).TotalMilliseconds < 400)
            {
                _control.Landed = values.Landed;
                OnPropertyChanged(nameof(LandedText));
                OnPropertyChanged(nameof(LandedTip));

                return;
            }

            _control.IsRange = values.IsRange;
            _control.Lo = values.Lo;
            _control.Hi = values.Hi;
            _control.Landed = values.Landed;
            _control.Integer = values.Integer;

            OnPropertyChanged(nameof(IsRange));
            OnPropertyChanged(nameof(IsSingle));
            OnPropertyChanged(nameof(CountValue));
            OnPropertyChanged(nameof(CountText));
            OnPropertyChanged(nameof(SelectedOption));
            OnPropertyChanged(nameof(IsOn));
            RaiseValues();
        }

        private void RaiseValues()
        {
            OnPropertyChanged(nameof(LoPosition));
            OnPropertyChanged(nameof(HiPosition));
            OnPropertyChanged(nameof(LoText));
            OnPropertyChanged(nameof(HiText));
            OnPropertyChanged(nameof(LandedText));
            OnPropertyChanged(nameof(LandedTip));
        }

        private void PushRanged()
        {
            _editedAt = DateTime.UtcNow;

            uint index = Index;
            bool isRange = _control.IsRange;
            float lo = _control.Lo;
            float hi = _control.Hi;

            _panel.Post(state => state.SetControlRanged(index, isRange, lo, hi));
        }

        private void PushInteger()
        {
            _editedAt = DateTime.UtcNow;

            uint index = Index;
            uint integer = _control.Integer;

            _panel.Post(state => state.SetControlInteger(index, integer));
        }

        private bool IsLogarithmic => _control.Log && _control.Min > 0.0f && _control.Max > _control.Min;

        private double ToPosition(float value)
        {
            double position = IsLogarithmic
                ? Math.Log(value / _control.Min) / Math.Log(_control.Max / _control.Min)
                : (value - _control.Min) / Math.Max(1e-9, _control.Max - _control.Min);

            return double.IsFinite(position) ? Math.Clamp(position, 0.0, 1.0) : 0.0;
        }

        private float FromPosition(double position)
        {
            return IsLogarithmic
                ? (float)(_control.Min * Math.Pow(_control.Max / _control.Min, position))
                : (float)(_control.Min + (_control.Max - _control.Min) * position);
        }

        private static bool TryParse(string? text, out float value)
        {
            return float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value) && float.IsFinite(value);
        }

        private static string Short(float value)
        {
            float magnitude = Math.Abs(value);
            string format = magnitude >= 100 ? "0" : magnitude >= 10 ? "0.0" : magnitude >= 1 ? "0.00" : magnitude >= 0.1 ? "0.000" : "0.0000";

            return value.ToString(format, CultureInfo.InvariantCulture);
        }
    }

    public sealed class ArborGroupViewModel
    {
        public string Title { get; }
        public bool HasTitle => !string.IsNullOrEmpty(Title);
        public ObservableCollection<ArborControlRowViewModel> Rows { get; } = new();

        public ArborGroupViewModel(string title)
        {
            Title = title;
        }
    }

    public sealed class ArborSectionViewModel
    {
        public string Title { get; }
        public ObservableCollection<ArborGroupViewModel> Groups { get; } = new();

        public ArborSectionViewModel(string title)
        {
            Title = title;
        }
    }

    public class ArborPanelViewModel : EditorPanelViewModel
    {
        private readonly Action<Action<EditorArborState>> _post;

        private readonly Dictionary<uint, ArborControlRowViewModel> _rows = new();

        // an edit goes on the undo stack once the controls have been still for a moment
        private readonly DispatcherTimer _commitTimer;

        private ArborSnapshot _state = new();

        public uint? LayoutVersion { get; private set; }
        public string TargetName => _state.TargetName;

        public ObservableCollection<string> Presets { get; } = new();
        public ObservableCollection<ArborSectionViewModel> Sections { get; } = new();

        public string Heading => _state.IsNewPlant ? $"{_state.TargetName} (new)" : _state.TargetName;
        public string BakeText => _state.VariantCount > 1
            ? $"Bake {_state.VariantCount} Variants"
            : _state.IsNewPlant ? "Bake" : "Bake Changes";

        public bool CanSetVariantCount => _state.CanSetVariantCount;

        public string VariantCountText
        {
            get => _state.VariantCount.ToString(CultureInfo.InvariantCulture);
            set
            {
                if (uint.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint count) && count != _state.VariantCount)
                {
                    _post(state => state.SetVariantCount(count));
                }
            }
        }
        public string StatusText => _state.StatusText;
        public bool HasStatus => !string.IsNullOrEmpty(_state.StatusText);
        public bool IsGenerating => _state.IsGenerating;

        private string? _selectedPreset;
        public string? SelectedPreset
        {
            get => _selectedPreset;
            set
            {
                if (value != null && value != _selectedPreset && SetProperty(ref _selectedPreset, value))
                {
                    Post(state => state.ApplyPreset(value));
                }
            }
        }

        public string SeedText
        {
            get => _state.Seed.ToString(CultureInfo.InvariantCulture);
            set
            {
                if (uint.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint seed) && seed != _state.Seed)
                {
                    Post(state => state.SetSeed(seed));
                }
            }
        }

        public ICommand RandomizeSeedCommand { get; }
        public ICommand BakeCommand { get; }
        public ICommand DiscardCommand { get; }

        public ArborPanelViewModel(Action<Action<EditorArborState>> post, Action? onClosed)
            : base("Arbor", onClosed)
        {
            _post = post;

            _commitTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
            _commitTimer.Tick += (_, _) =>
            {
                _commitTimer.Stop();
                _post(state => state.CommitEdit());
            };

            RandomizeSeedCommand = new RelayCommand(() => _post(state => state.RandomizeSeed()));
            BakeCommand = new RelayCommand(() => _post(state => state.Bake()), () => _state.CanBake);
            DiscardCommand = new RelayCommand(() => _post(state => state.Exit(/* apply */ false)));
        }

        internal void Post(Action<EditorArborState> action)
        {
            _post(action);

            _commitTimer.Stop();
            _commitTimer.Start();
        }

        internal void Update(ArborSnapshot snapshot)
        {
            bool targetChanged = snapshot.TargetName != _state.TargetName || snapshot.Kind != _state.Kind;

            _state = snapshot;

            if (targetChanged || !Presets.SequenceEqual(snapshot.Presets))
            {
                Presets.Clear();

                foreach (string preset in snapshot.Presets)
                {
                    Presets.Add(preset);
                }
            }

            string? selectedPreset = Presets.Contains(snapshot.RecipePreset) ? snapshot.RecipePreset : null;

            if (selectedPreset != _selectedPreset)
            {
                _selectedPreset = selectedPreset;
                OnPropertyChanged(nameof(SelectedPreset));
            }

            if (snapshot.HasLayout)
            {
                RebuildRows(snapshot.Controls);
                LayoutVersion = snapshot.LayoutVersion;
            }
            else
            {
                foreach (ArborControlSnapshot control in snapshot.Controls)
                {
                    if (_rows.TryGetValue(control.Index, out ArborControlRowViewModel? row))
                    {
                        row.Update(control);
                    }
                }
            }

            OnPropertyChanged(nameof(Heading));
            OnPropertyChanged(nameof(BakeText));
            OnPropertyChanged(nameof(CanSetVariantCount));
            OnPropertyChanged(nameof(VariantCountText));
            OnPropertyChanged(nameof(StatusText));
            OnPropertyChanged(nameof(HasStatus));
            OnPropertyChanged(nameof(IsGenerating));
            OnPropertyChanged(nameof(SeedText));
            (BakeCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        private void RebuildRows(ArborControlSnapshot[] controls)
        {
            Sections.Clear();
            _rows.Clear();

            ArborSectionViewModel? section = null;
            ArborGroupViewModel? group = null;

            foreach (ArborControlSnapshot control in controls)
            {
                if (section == null || section.Title != control.Section)
                {
                    section = new ArborSectionViewModel(control.Section);
                    Sections.Add(section);
                    group = null;
                }

                if (group == null || group.Title != control.Group)
                {
                    group = new ArborGroupViewModel(control.Group);
                    section.Groups.Add(group);
                }

                ArborControlRowViewModel row = new ArborControlRowViewModel(this, control);
                group.Rows.Add(row);
                _rows[control.Index] = row;
            }
        }
    }
}
