using System;
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

        private readonly Node _node;
        private readonly Property _flagsProperty;

        public MobilityPropertyViewModel(Node node, Property flagsProperty)
            : base("Mobility", flagsProperty.TypeInfo, null, null, false)
        {
            _node = node;
            _flagsProperty = flagsProperty;
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
                ulong currentValue;

                try
                {
                    using BoxedValue boxed = _flagsProperty.Get(_node);
                    object? rawValue = boxed.GetValue();
                    currentValue = rawValue != null ? Convert.ToUInt64(rawValue) : 0ul;
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
                            ulong mobilityField = currentValue & MobilityMask;

                            _mobilityValue = (uint)mobilityField;

                            _isStatic = mobilityField == MobilityStatic;
                            _isDynamic = mobilityField == MobilityDynamic;
                            _isInherit = !_isStatic && !_isDynamic;

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

            Node capturedNode = _node;
            Property capturedProperty = _flagsProperty;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Read current flags
                    using BoxedValue currentBoxed = capturedProperty.Get(capturedNode);
                    object? currentRaw = currentBoxed.GetValue();
                    ulong currentFlags = currentRaw != null ? Convert.ToUInt64(currentRaw) : 0ul;

                    ulong newValue = (currentFlags & ~MobilityMask) | mobilityValue;
                    ulong oldValue = currentFlags;

                    if (newValue == oldValue)
                    {
                        Dispatcher.UIThread.Post(RefreshValue);
                        return;
                    }

                    EditorProject? project = EngineManager.CurrentProject;

                    void ApplyValue(ulong value)
                    {
                        try
                        {
                            using BoxedValue bv = new BoxedValue(value);
                            capturedProperty.Set(capturedNode, bv);
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(LogLevel.Warning, $"Inspector failed to set mobility: {ex.Message}");
                        }

                        Dispatcher.UIThread.Post(() =>
                        {
                            RefreshValue();
                            ValueChangedCallback?.Invoke();
                        });
                    }

                    EditorAction action = new EditorAction(
                        "Set Mobility",
                        execute: (_, _) => ApplyValue(newValue),
                        revert: (_, _) => ApplyValue(oldValue)
                    );

                    if (project != null)
                    {
                        // PushAction executes the action, so don't apply it a second time here.
                        project.ActionStack.PushAction(action);
                    }
                    else
                    {
                        ApplyValue(newValue);
                    }
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
