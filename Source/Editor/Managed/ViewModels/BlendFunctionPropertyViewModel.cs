using System;
using System.Collections.Generic;
using System.Linq;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public sealed class BlendPreset
    {
        public BlendPreset(string name, string description, BlendFunction value)
        {
            Name = name;
            Description = description;
            Value = value;
        }

        public string Name { get; }

        public string Description { get; }

        public BlendFunction Value { get; }
    }

    public class BlendFunctionPropertyViewModel : InspectorPropertyViewModelBase
    {
        public static readonly IReadOnlyList<BlendPreset> Presets = new List<BlendPreset>
        {
            new BlendPreset(
                "Opaque",
                "No blending",
                BlendFunction.None()),
            new BlendPreset(
                "Normal",
                "The source is blended over the destination by its alpha",
                BlendFunction.AlphaBlending()),
            new BlendPreset(
                "Premultiplied Alpha",
                "For sources whose color channels are already multiplied by their own alpha",
                BlendFunction.PremultipliedAlpha()),
            new BlendPreset(
                "Additive (Linear Dodge)",
                "Adds the source on top of the destination",
                BlendFunction.Additive()),
            new BlendPreset(
                "Multiply",
                "Multiplies source and destination (darkens)",
                new BlendFunction(BlendModeFactor.DstColor, BlendModeFactor.Zero, BlendModeFactor.DstAlpha, BlendModeFactor.Zero)),
            new BlendPreset(
                "Screen",
                "Multiplies the inverse of the pixel values of the source and the destination (lightens)",
                new BlendFunction(BlendModeFactor.One, BlendModeFactor.OneMinusSrcColor, BlendModeFactor.One, BlendModeFactor.OneMinusSrcAlpha)),
        };

        public static readonly IReadOnlyList<BlendModeFactor> FactorOptions = Enum.GetValues<BlendModeFactor>()
            .Where(factor => factor != BlendModeFactor.Max)
            .ToArray();

        private BlendFunction _current = BlendFunction.None();

        private BlendPreset? _selectedPreset;
        private bool _isCustom;

        private bool _isPresetMode = true;
        private bool _isAdvancedMode;
        private bool _suppressModeProtection;
        private bool _modeInitialized;

        public override bool ShowInlineLabel => false;
        public override bool SupportsMultipleTargets => false;
        public override bool ShowsDescriptionInOwnTemplate => true;

        private bool _isExpanded = true;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public BlendFunctionPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
        }

        public BlendFunctionPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
        }

        public BlendFunctionPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
        }

        public IReadOnlyList<BlendPreset> PresetOptions => Presets;

        public IReadOnlyList<BlendModeFactor> FactorOptionList => FactorOptions;

        public bool IsCustom
        {
            get => _isCustom;
            private set => SetProperty(ref _isCustom, value);
        }

        public BlendPreset? SelectedPreset
        {
            get => _selectedPreset;
            set
            {
                if (!SetProperty(ref _selectedPreset, value) || IsApplyingModelValue || value == null || _isReadOnly)
                {
                    return;
                }

                CommitBlendFunction(value.Value, $"Set {Label}: {value.Name}");
            }
        }

        public bool IsPresetMode
        {
            get => _isPresetMode;
            set
            {
                if (!_suppressModeProtection && !value && _isPresetMode)
                {
                    _isPresetMode = true;
                    OnPropertyChanged();
                    return;
                }

                if (SetProperty(ref _isPresetMode, value) && value)
                {
                    _suppressModeProtection = true;
                    IsAdvancedMode = false;
                    _suppressModeProtection = false;
                }
            }
        }

        public bool IsAdvancedMode
        {
            get => _isAdvancedMode;
            set
            {
                if (!_suppressModeProtection && !value && _isAdvancedMode)
                {
                    _isAdvancedMode = true;
                    OnPropertyChanged();
                    return;
                }

                if (SetProperty(ref _isAdvancedMode, value) && value)
                {
                    _suppressModeProtection = true;
                    IsPresetMode = false;
                    _suppressModeProtection = false;
                }
            }
        }

        public BlendModeFactor SrcColor
        {
            get => _current.SrcColor;
            set => CommitFactor(value, _current.SrcColor, (ref BlendFunction bf) => bf.SrcColor = value, "SrcColor");
        }

        public BlendModeFactor DstColor
        {
            get => _current.DstColor;
            set => CommitFactor(value, _current.DstColor, (ref BlendFunction bf) => bf.DstColor = value, "DstColor");
        }

        public BlendModeFactor SrcAlpha
        {
            get => _current.SrcAlpha;
            set => CommitFactor(value, _current.SrcAlpha, (ref BlendFunction bf) => bf.SrcAlpha = value, "SrcAlpha");
        }

        public BlendModeFactor DstAlpha
        {
            get => _current.DstAlpha;
            set => CommitFactor(value, _current.DstAlpha, (ref BlendFunction bf) => bf.DstAlpha = value, "DstAlpha");
        }

        private delegate void FactorSetter(ref BlendFunction blendFunction);

        private void CommitFactor(BlendModeFactor newFactor, BlendModeFactor oldFactor, FactorSetter apply, string factorName)
        {
            if (IsApplyingModelValue || _isReadOnly || newFactor == oldFactor)
            {
                return;
            }

            BlendFunction updated = _current;
            apply(ref updated);

            CommitBlendFunction(updated, $"Set {Label}.{factorName}");
        }

        private void CommitBlendFunction(BlendFunction newValue, string actionText)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(newValue);
                    CommitPropertyChange(actionText, boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to set blend function '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                BlendFunction current;

                try
                {
                    using BoxedValue boxed = GetPropertyValue();
                    current = boxed.GetValue() is BlendFunction blendFunction ? blendFunction : BlendFunction.None();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read blend function '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() => ApplyCurrent(current));
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private void ApplyCurrent(BlendFunction current)
        {
            _current = current;

            BlendPreset? matched = Presets.FirstOrDefault(preset => preset.Value == current);

            _selectedPreset = matched;
            OnPropertyChanged(nameof(SelectedPreset));

            IsCustom = matched == null;
            Value = matched?.Name ?? "Custom";

            OnPropertyChanged(nameof(SrcColor));
            OnPropertyChanged(nameof(DstColor));
            OnPropertyChanged(nameof(SrcAlpha));
            OnPropertyChanged(nameof(DstAlpha));

            if (!_modeInitialized)
            {
                _modeInitialized = true;

                _suppressModeProtection = true;
                _isPresetMode = matched != null;
                _isAdvancedMode = !_isPresetMode;
                _suppressModeProtection = false;

                OnPropertyChanged(nameof(IsPresetMode));
                OnPropertyChanged(nameof(IsAdvancedMode));
            }
        }
    }
}
