using System;
using System.Collections.Generic;
using System.Threading;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor;

namespace Hyperion.Editor.ViewModels
{
    public class MobilityPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const uint MobilityMask = (uint)NodeFlags.Mobility;

        private const uint MobilityStatic = (uint)NodeFlags.MobilityStatic;
        private const uint MobilityStaticByProxy = (uint)NodeFlags.MobilityStaticByProxy;
        private const uint MobilityDynamic = (uint)NodeFlags.MobilityDynamic;

        private bool _isInherit = false;
        private bool _isStatic = false;
        private bool _isDynamic = false;
        private bool _suppressProtection = false;

        private uint _mobilityValue = 0;

        public MobilityPropertyViewModel(Node node, Property flagsProperty)
            : base("Mobility", flagsProperty.TypeInfo, () => flagsProperty.Get(node), value => flagsProperty.Set(node, value), false)
        {
        }

        public bool IsInherit
        {
            get => _isInherit;
            set
            {
                if (!_suppressProtection && !value && _isInherit)
                {
                    _isInherit = true;
                    OnPropertyChanged();
                    return;
                }

                if (SetProperty(ref _isInherit, value) && value)
                {
                    _suppressProtection = true;
                    IsStatic = false;
                    IsDynamic = false;
                    _suppressProtection = false;

                    CommitMobility(0L);
                }
            }
        }

        public bool IsStatic
        {
            get => _isStatic;
            set
            {
                if (!_suppressProtection && !value && _isStatic)
                {
                    _isStatic = true;
                    OnPropertyChanged();
                    return;
                }

                if (SetProperty(ref _isStatic, value) && value)
                {
                    _suppressProtection = true;
                    IsDynamic = false;
                    IsInherit = false;
                    _suppressProtection = false;

                    CommitMobility(MobilityStatic);
                }
            }
        }
        public bool IsStaticInherited => IsInherit && (_mobilityValue & MobilityStaticByProxy) != 0;

        public bool IsDynamic
        {
            get => _isDynamic;
            set
            {
                if (!_suppressProtection && !value && _isDynamic)
                {
                    _isDynamic = true;
                    OnPropertyChanged();
                    return;
                }

                if (SetProperty(ref _isDynamic, value) && value)
                {
                    _suppressProtection = true;
                    IsStatic = false;
                    IsInherit = false;
                    _suppressProtection = false;

                    CommitMobility(MobilityDynamic);
                }
            }
        }

        public bool IsDynamicInherited => IsInherit && (_mobilityValue == MobilityDynamic);

        public override void RefreshValue()
        {
            if (!BeginRefresh())
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                List<ulong> mobilityFields;

                try
                {
                    mobilityFields = ReadAllTargets(boxed =>
                    {
                        object? rawValue = boxed.GetValue();
                        return (rawValue != null ? Convert.ToUInt64(rawValue) : 0ul) & MobilityMask;
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read mobility: {ex.Message}");

                    EndRefresh();

                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            ulong mobilityField = mobilityFields[0];

                            // No option is shown selected when the selected nodes disagree.
                            bool isShared = mobilityFields.TrueForAll(field => field == mobilityField);

                            _mobilityValue = (uint)mobilityField;

                            _isStatic = isShared && mobilityField == MobilityStatic;
                            _isDynamic = isShared && mobilityField == MobilityDynamic;
                            _isInherit = isShared && !_isStatic && !_isDynamic;

                            HasMixedValues = !isShared;

                            OnPropertyChanged(nameof(IsInherit));
                            OnPropertyChanged(nameof(IsStatic));
                            OnPropertyChanged(nameof(IsDynamic));
                            OnPropertyChanged(nameof(IsStaticInherited));
                            OnPropertyChanged(nameof(IsDynamicInherited));
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private void CommitMobility(ulong mobilityValue)
        {
            if (IsApplyingModelValue)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Only the mobility bits change; each node keeps the rest of its flags.
                    CommitPropertyChange("Set Mobility", current =>
                    {
                        object? currentRaw = current.GetValue();
                        ulong currentFlags = currentRaw != null ? Convert.ToUInt64(currentRaw) : 0ul;

                        return ToIntegralTypeOf((currentFlags & ~MobilityMask) | mobilityValue, currentRaw);
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to set mobility: {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }
    }
}
