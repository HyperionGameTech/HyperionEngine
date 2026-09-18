using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public abstract class InspectorPropertyViewModelBase : ViewModelBase
    {
        private const int RefreshIdle = 0;
        private const int RefreshRunning = 1;
        private const int RefreshRunningWithPending = 2;

        protected readonly ObjectBase? _target;
        protected readonly Property _property;
        protected readonly bool _isReadOnly;

        private readonly IntPtr _componentClassAddress;
        private readonly Func<IntPtr>? _componentTargetResolver;

        // Delegate-based path (used by e.g. array-element VMs where there is no Property).
        private readonly Func<BoxedValue>? _valueGetter;
        private readonly Action<BoxedValue>? _valueSetter;
        protected readonly TypeInfo _typeInfoHint;

        private string _value = string.Empty;
        private string _label = string.Empty;
        private string _description = string.Empty;

        private int _refreshState;
        private int _applyingModelValue;

        private bool _isEditing;

        private bool _isOverridden;
        /// <summary>True when at least one swatch's override set contains this property.</summary>
        public bool IsOverridden
        {
            get => _isOverridden;
            private set => SetProperty(ref _isOverridden, value);
        }

        private string _overrideSignifier = string.Empty;
        /// <summary>Small text under the label, e.g. "SwatchA, SwatchB override this value".</summary>
        public string OverrideSignifier
        {
            get => _overrideSignifier;
            private set => SetProperty(ref _overrideSignifier, value);
        }

        private string? _overrideTooltip;
        /// <summary>Hover text for the row's override marker; null when nothing overrides the property, so no empty tooltip pops up.</summary>
        public string? OverrideTooltip
        {
            get => _overrideTooltip;
            private set => SetProperty(ref _overrideTooltip, value);
        }

        private bool _isOverriddenByCurrentSwatch;
        /// <summary>True when the World's active swatch's override set contains this property; shows the per-row revert button.</summary>
        public bool IsOverriddenByCurrentSwatch
        {
            get => _isOverriddenByCurrentSwatch;
            private set => SetProperty(ref _isOverriddenByCurrentSwatch, value);
        }

        private bool _isOverriddenByOtherSwatchOnly;
        /// <summary>True when only swatches other than the active one override this property; draws the marker muted.</summary>
        public bool IsOverriddenByOtherSwatchOnly
        {
            get => _isOverriddenByOtherSwatchOnly;
            private set => SetProperty(ref _isOverriddenByOtherSwatchOnly, value);
        }

        /// <summary>True for rows backed by a real object + Property (entity-level rows), i.e. the rows that support per-swatch overrides.</summary>
        public bool IsEntityLevelRow => _valueGetter == null && _componentTargetResolver == null;

        /// <summary>Called by the owning inspector after querying which swatches override this property.</summary>
        internal void SetOverrideInfo(List<string> swatchNames, string? currentSwatchName = null)
        {
            IsOverridden = swatchNames.Count > 0;
            OverrideSignifier = swatchNames.Count > 0 ? $"{string.Join(", ", swatchNames)} override this value" : string.Empty;
            OverrideTooltip = swatchNames.Count > 0 ? $"Overridden in: {string.Join(", ", swatchNames)}" : null;
            IsOverriddenByCurrentSwatch = currentSwatchName != null && swatchNames.Contains(currentSwatchName);
            IsOverriddenByOtherSwatchOnly = IsOverridden && !IsOverriddenByCurrentSwatch;

            OnPropertyChanged(nameof(LabelTooltip));
        }

        public Property Property => _property;

        public string Label => _label;

        /// <summary>From the property's Description attribute; empty when it has none.</summary>
        public string Description => _description;

        public bool HasDescription => _description.Length != 0;

        /// <summary>True for editors whose template places the description itself (e.g. under an expander header) rather than below the row.</summary>
        public virtual bool ShowsDescriptionInOwnTemplate => false;

        public bool ShowDescriptionBelowRow => HasDescription && !ShowsDescriptionInOwnTemplate;

        /// <summary>Hover text for the label: the description, followed by which swatches override the value. Null when there's neither.</summary>
        public string? LabelTooltip
        {
            get
            {
                if (!HasDescription)
                {
                    return OverrideTooltip;
                }

                return OverrideTooltip == null ? _description : $"{_description}\n\n{OverrideTooltip}";
            }
        }

        public string Value
        {
            get => _value;
            protected set => SetProperty(ref _value, value);
        }

        public virtual bool IsTextEditable => false;

        public virtual bool IsEnumEditable => false;

        public virtual bool IsEnumFlagsEditable => false;

        public virtual bool IsNumericEditable => false;

        public virtual bool ShowInlineLabel => true;

        public bool ShowTextValue => !IsTextEditable && !IsEnumEditable && !IsEnumFlagsEditable && !IsNumericEditable;

        public bool IsEditing
        {
            get => _isEditing;
            set => _isEditing = value;
        }

        // Called on the sim thread immediately before a property value write, so container VMs
        // can re-read their cached copy of the containing value.
        public virtual Action? PreWriteCallback { get; set; }

        // Called on the sim thread immediately after a property value write succeeds.
        public virtual Action? PostWriteCallback { get; set; }

        // Called on the UI thread after a value write (including undo/redo) has been applied, so the
        // owning object's other properties can re-read.
        public virtual Action? ValueChangedCallback { get; set; }

        protected InspectorPropertyViewModelBase(ObjectBase? target, Property property, bool isReadOnly = false)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _property = property;
            _isReadOnly = isReadOnly;

            InitializeLabelAndDescription(property);
        }

        protected InspectorPropertyViewModelBase(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly = false)
        {
            _target = null;
            _componentClassAddress = classAddress;
            _componentTargetResolver = targetAddressResolver ?? throw new ArgumentNullException(nameof(targetAddressResolver));
            _property = property;
            _isReadOnly = isReadOnly;

            InitializeLabelAndDescription(property);
        }

        /// <summary>Delegate-based constructor used when there is no backing Property (e.g. array elements).</summary>
        protected InspectorPropertyViewModelBase(string label, TypeInfo typeInfoHint, Func<BoxedValue>? getter, Action<BoxedValue>? setter, bool isReadOnly = false)
        {
            _target = null;
            _property = Property.Invalid;
            _isReadOnly = isReadOnly;
            _valueGetter = getter;
            _valueSetter = setter;
            _typeInfoHint = typeInfoHint;
            _label = label;
        }

        private void InitializeLabelAndDescription(Property property)
        {
            ClassAttribute? attrLabel = property.GetAttribute("label");

            if (attrLabel != null)
            {
                _label = attrLabel.Value.GetString();
            }
            else
            {
                _label = property.Name.ToString();
            }

            ClassAttribute? attrDescription = property.GetAttribute("description");

            _description = attrDescription?.GetString() ?? string.Empty;
        }

        /// <summary>
        /// Claims the right to start a read. Returns false when a read is already in flight, in
        /// which case it is re-run by <see cref="EndRefresh"/> so no refresh request is lost.
        /// </summary>
        protected bool BeginRefresh()
        {
            while (true)
            {
                int state = Volatile.Read(ref _refreshState);

                if (state == RefreshIdle)
                {
                    if (Interlocked.CompareExchange(ref _refreshState, RefreshRunning, RefreshIdle) == RefreshIdle)
                    {
                        return true;
                    }
                }
                else if (Interlocked.CompareExchange(ref _refreshState, RefreshRunningWithPending, state) == state)
                {
                    return false;
                }
            }
        }

        protected void EndRefresh()
        {
            if (Interlocked.Exchange(ref _refreshState, RefreshIdle) == RefreshRunningWithPending)
            {
                Dispatcher.UIThread.Post(RefreshValue);
            }
        }

        /// <summary>Applies model values to UI-bound fields without the bindings echoing them back.</summary>
        protected void ApplyModelValue(Action apply)
        {
            Interlocked.Increment(ref _applyingModelValue);

            try
            {
                apply();
            }
            finally
            {
                Interlocked.Decrement(ref _applyingModelValue);
            }
        }

        protected bool IsApplyingModelValue => Volatile.Read(ref _applyingModelValue) != 0;

        protected BoxedValue GetPropertyValue()
        {
            if (_valueGetter != null)
                return _valueGetter();

            if (_componentTargetResolver != null)
            {
                return _property.Get(_componentClassAddress, _componentTargetResolver());
            }

            return _property.Get(_target!);
        }

        protected void SetPropertyValue(BoxedValue value)
        {
            if (_valueSetter != null)
            {
                _valueSetter(value);
                PostWriteCallback?.Invoke();
                return;
            }

            if (_componentTargetResolver != null)
            {
                _property.Set(_componentClassAddress, _componentTargetResolver(), value);
            }
            else
            {
                _property.Set(_target!, value);
            }

            PostWriteCallback?.Invoke();
        }

        protected bool HasObjectContext => _valueGetter == null;

        protected Property? FindSiblingProperty(Name name)
        {
            if (_componentTargetResolver != null)
            {
                return new Class(_componentClassAddress).GetProperty(name);
            }

            if (_target != null)
            {
                return _target.Class.GetProperty(name);
            }

            return null;
        }

        protected BoxedValue GetSiblingPropertyValue(Property property)
        {
            if (_componentTargetResolver != null)
            {
                return property.Get(_componentClassAddress, _componentTargetResolver());
            }

            if (_target != null)
            {
                return property.Get(_target);
            }

            throw new InvalidOperationException("No object context is available to read a sibling property.");
        }

        protected void SetSiblingPropertyValue(Property property, BoxedValue value)
        {
            if (_componentTargetResolver != null)
            {
                property.Set(_componentClassAddress, _componentTargetResolver(), value);
            }
            else if (_target != null)
            {
                property.Set(_target, value);
            }
            else
            {
                throw new InvalidOperationException("No object context is available to write a sibling property.");
            }

            PostWriteCallback?.Invoke();
        }

        protected bool TryWriteContainerValueToSwatchOverride(BoxedValue value)
        {
            if (_valueSetter != null || _componentTargetResolver != null || _target is not Entity)
            {
                return false;
            }

            if (_property.Name == new Name("Name", weak: true))
            {
                return false;
            }

            if (SwatchOverrideEditContext.CurrentEntity is not Entity overrideEntity
                || !overrideEntity.IsValid
                || _target.NativeAddress != overrideEntity.NativeAddress
                || SwatchOverrideEditContext.ActiveSwatchName is not string contextSwatch)
            {
                return false;
            }

            Name swatch = new Name(contextSwatch);

            if (!SwatchOverrideEditContext.OverrideModeActive
                && !EntitySwatchOverrides.IsPropertyOverridden(overrideEntity, swatch, _property.Name))
            {
                return false;
            }

            if (!EntitySwatchOverrides.HasSet(overrideEntity, swatch))
            {
                EntitySwatchOverrides.AddSet(overrideEntity, swatch);
            }

            if (EntitySwatchOverrides.GetAppliedSwatch(overrideEntity).HashCode != swatch.HashCode)
            {
                EntitySwatchOverrides.Apply(overrideEntity, swatch);
            }

            EntitySwatchOverrides.SetValue(overrideEntity, swatch, _property.Name, value);

            return true;
        }

        /// <summary>
        /// Writes a new value through the project's action stack. Must be called on the sim thread.
        /// </summary>
        protected void CommitPropertyChange(string actionText, BoxedValue newValue)
        {
            if (_isReadOnly)
            {
                return;
            }

            // Bring any cached copy of the containing value up to date before reading the old value,
            // otherwise both the equality check below and the write itself use a stale snapshot.
            PreWriteCallback?.Invoke();

            object? oldValueObj = null;
            bool oldValueCaptured = false;

            try
            {
                using BoxedValue old = GetPropertyValue();
                oldValueObj = old.GetValue();
                oldValueCaptured = true;
            }
            catch
            {
                /* If we can't read the old value, undo will be a no-op */
            }

            object? newValueObj = newValue.GetValue();

            if (oldValueCaptured && Equals(oldValueObj, newValueObj))
            {
                // Nothing changed, but the UI may be showing an unparsed/optimistic value - put it back in sync.
                Dispatcher.UIThread.Post(RefreshValue);
                return;
            }

            EditorProject? project = EngineManager.CurrentProject;
            Debug.Assert(project != null, "No active project found when committing property change");

            // Capture all members we need so we don't need to actually capture 'this'
            IntPtr capturedClassAddress = _componentClassAddress;
            Func<IntPtr>? capturedResolver = _componentTargetResolver;
            ObjectBase? capturedTarget = _target;
            Property capturedProperty = _property;
            Action<BoxedValue>? capturedSetter = _valueSetter;
            InspectorPropertyViewModelBase capturedThis = this;

            ///Swatch override routing
            if (capturedSetter == null && capturedResolver == null
                && capturedProperty.Name != new Name("Name", weak: true)
                && SwatchOverrideEditContext.CurrentEntity is Entity overrideEntity
                && overrideEntity.IsValid
                && capturedTarget != null
                && capturedTarget.NativeAddress == overrideEntity.NativeAddress
                && SwatchOverrideEditContext.ActiveSwatchName is string contextSwatch)
            {
                // Edits target the active swatch's override when override mode is enabled, or
                // when the property is ALREADY overridden by that swatch (editing the existing
                // override directly). Otherwise the edit writes the base value.
                if (SwatchOverrideEditContext.OverrideModeActive
                    || EntitySwatchOverrides.IsPropertyOverridden(overrideEntity, new Name(contextSwatch), capturedProperty.Name))
                {
                    CommitSwatchOverrideChange(overrideEntity, contextSwatch, actionText, newValueObj);
                    return;
                }
            }

            void ApplyValue(object? valueObj, bool hasValue)
            {
                if (!hasValue)
                {
                    return;
                }

                try
                {
                    // Undo/redo runs long after the original edit, so the cached copy has to be
                    // re-read here too.
                    capturedThis.PreWriteCallback?.Invoke();

                    using BoxedValue bv = new BoxedValue(valueObj);

                    if (capturedSetter != null)
                    {
                        capturedSetter(bv);
                    }
                    else if (capturedResolver != null)
                    {
                        capturedProperty.Set(capturedClassAddress, capturedResolver(), bv);
                    }
                    else
                    {
                        capturedProperty.Set(capturedTarget!, bv);
                    }

                    capturedThis.PostWriteCallback?.Invoke();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Editor action failed for property '{capturedThis.Label}': {ex.Message}");
                }

                Dispatcher.UIThread.Post(() =>
                {
                    capturedThis.RefreshValue();
                    capturedThis.ValueChangedCallback?.Invoke();
                });
            }

            EditorAction action = new EditorAction(
                actionText,
                execute: (_, _) => ApplyValue(newValueObj, hasValue: true),
                revert:  (_, _) => ApplyValue(oldValueObj, hasValue: oldValueCaptured)
            );

            if (project != null)
            {
                project.ActionStack.PushAction(action);
            }
            else
            {
                // No project to record undo against - still apply the edit.
                ApplyValue(newValueObj, hasValue: true);
            }
        }

        public virtual void CommitValue() { }

        /// <summary>
        /// Routes a property edit into the World's active swatch's override set for the entity
        /// (auto-creating and applying the set if needed). Reached when override mode is enabled
        /// or when the property is already overridden by that swatch. Writing the base value
        /// prunes the override entry; undo/redo restores the previous override state.
        /// Must be called on the sim thread.
        /// </summary>
        private void CommitSwatchOverrideChange(Entity entity, string swatchName, string actionText, object? newValueObj)
        {
            Name swatch = new Name(swatchName);
            Name propertyName = _property.Name;

            // Ensure the set exists for the active swatch and is applied, so the edit is visible
            if (!EntitySwatchOverrides.HasSet(entity, swatch))
            {
                EntitySwatchOverrides.AddSet(entity, swatch);
            }

            if (EntitySwatchOverrides.GetAppliedSwatch(entity).HashCode != swatch.HashCode)
            {
                EntitySwatchOverrides.Apply(entity, swatch);
            }

            bool wasOverridden = EntitySwatchOverrides.IsPropertyOverridden(entity, swatch, propertyName);

            object? baseValueObj = null;
            bool hasBaseValue = EntitySwatchOverrides.GetBaseValue(entity, swatch, propertyName, out BoxedValue baseValue);

            if (hasBaseValue)
            {
                try
                {
                    baseValueObj = baseValue.GetValue();
                }
                catch
                {
                    hasBaseValue = false;
                }
                finally
                {
                    baseValue.Dispose();
                }
            }

            object? previousOverrideObj = null;
            bool hadPreviousOverride = false;

            if (wasOverridden)
            {
                hadPreviousOverride = EntitySwatchOverrides.GetValue(entity, swatch, propertyName, out BoxedValue previousOverride);

                if (hadPreviousOverride)
                {
                    try
                    {
                        previousOverrideObj = previousOverride.GetValue();
                    }
                    catch
                    {
                        hadPreviousOverride = false;
                    }
                    finally
                    {
                        previousOverride.Dispose();
                    }
                }
            }

            // Writing the base value (or the exact current override) is a no-op
            if (!wasOverridden && hasBaseValue && Equals(baseValueObj, newValueObj))
            {
                return;
            }

            if (wasOverridden && hadPreviousOverride && Equals(previousOverrideObj, newValueObj) && !Equals(baseValueObj, newValueObj))
            {
                return;
            }

            // Writing the base value over an existing override removes (prunes) the override
            bool removeOverride = hasBaseValue && Equals(baseValueObj, newValueObj);

            Entity capturedEntity = entity;
            Name capturedSwatch = swatch;

            void ApplyOverrideState(bool setOverride, object? valueObj, bool hasValue)
            {
                if (setOverride)
                {
                    if (!hasValue)
                    {
                        return;
                    }

                    using BoxedValue bv = new BoxedValue(valueObj);

                    EntitySwatchOverrides.SetValue(capturedEntity, capturedSwatch, propertyName, bv);
                }
                else
                {
                    EntitySwatchOverrides.RemoveValue(capturedEntity, capturedSwatch, propertyName);
                }
            }

            EditorAction action = new EditorAction(
                removeOverride ? $"Revert Override ({swatchName}): {Label}" : $"Override ({swatchName}): {Label}",
                execute: (_, _) => ApplyOverrideState(!removeOverride, newValueObj, true),
                revert: (_, _) =>
                {
                    if (wasOverridden && hadPreviousOverride)
                    {
                        ApplyOverrideState(true, previousOverrideObj, true);
                    }
                    else
                    {
                        ApplyOverrideState(false, null, false);
                    }
                });

            EditorProject? overrideProject = EngineManager.CurrentProject;

            if (overrideProject != null)
            {
                overrideProject.ActionStack.PushAction(action);
            }
            else
            {
                // No project to record undo against - still apply the edit.
                ApplyOverrideState(!removeOverride, newValueObj, true);
            }

            Dispatcher.UIThread.Post(() =>
            {
                RefreshValue();
                ValueChangedCallback?.Invoke();
            });
        }

        private ICommand? _revertOverrideCommand;
        public ICommand RevertOverrideCommand => _revertOverrideCommand ??= new RelayCommand(RevertOverride);

        /// <summary>
        /// Drops this property from the World's active swatch's override set, restoring the base
        /// value (the override entry is removed, not overwritten). Afterwards edits write the
        /// base value again - unless override mode is enabled, in which case a new override is
        /// created on the next edit. Only for entity-level rows currently overridden by the
        /// active swatch.
        /// </summary>
        private void RevertOverride()
        {
            if (!IsOverriddenByCurrentSwatch)
            {
                return;
            }

            if (_valueGetter != null || _componentTargetResolver != null)
            {
                return; // entity-level rows only
            }

            if (SwatchOverrideEditContext.CurrentEntity is not Entity entity || !entity.IsValid)
            {
                return;
            }

            if (SwatchOverrideEditContext.ActiveSwatchName is not string swatchName)
            {
                return;
            }

            Name swatch = new Name(swatchName);
            Name propertyName = _property.Name;
            string label = Label;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!EntitySwatchOverrides.IsPropertyOverridden(entity, swatch, propertyName))
                {
                    return;
                }

                object? previousOverrideObj = null;
                bool hadPreviousOverride = EntitySwatchOverrides.GetValue(entity, swatch, propertyName, out BoxedValue previousOverride);

                if (hadPreviousOverride)
                {
                    try
                    {
                        previousOverrideObj = previousOverride.GetValue();
                    }
                    catch
                    {
                        hadPreviousOverride = false;
                    }
                    finally
                    {
                        previousOverride.Dispose();
                    }
                }

                Entity capturedEntity = entity;
                Name capturedSwatch = swatch;

                // Removing an applied override makes the native side restore the base value
                void ApplyRemove()
                {
                    EntitySwatchOverrides.RemoveValue(capturedEntity, capturedSwatch, propertyName);
                }

                void ApplyRestore()
                {
                    if (hadPreviousOverride)
                    {
                        using BoxedValue value = new BoxedValue(previousOverrideObj);

                        EntitySwatchOverrides.SetValue(capturedEntity, capturedSwatch, propertyName, value);
                    }
                    else
                    {
                        EntitySwatchOverrides.RemoveValue(capturedEntity, capturedSwatch, propertyName);
                    }
                }

                EditorProject? project = EngineManager.CurrentProject;

                if (project != null)
                {
                    project.ActionStack.PushAction(new EditorAction(
                        $"Revert Override ({swatchName}): {label}",
                        (_, _) => ApplyRemove(),
                        (_, _) => ApplyRestore()));
                }
                else
                {
                    // No project to record undo against - still apply the revert.
                    ApplyRemove();
                }

                Dispatcher.UIThread.Post(() =>
                {
                    RefreshValue();
                    ValueChangedCallback?.Invoke();
                });
            });
        }

        private ICommand? _commitValueCommand;
        public ICommand CommitValueCommand => _commitValueCommand ??= new RelayCommand(CommitValue);

        protected bool IsTargetValid =>
            _valueGetter != null || _componentTargetResolver != null || (_target?.IsValid ?? false);

        public abstract void RefreshValue();

        internal static bool IsNameType(TypeInfo typeInfo)
        {
            // Name is a foundational type with no HYP_STRUCT reflection, so it never has a
            // Class - fall back to the compiler-parsed, namespace-qualified TypeInfo name.
            if (typeInfo.Class?.Name is Name typeName)
            {
                return typeName == "Name";
            }

            return typeInfo.Name == "Hyperion::Name";
        }

        protected static string FormatValue(object? value)
        {
            if (value == null)
            {
                return "(null)";
            }

            switch (value)
            {
                case string str:
                    return str;
                case bool boolean:
                    return boolean ? "True" : "False";
                case Enum enumValue:
                    return enumValue.ToString();
                case Name name:
                    return name.ToString();
                case byte[] bytes:
                    return $"{bytes.Length} byte(s)";
                case ObjectBase obj when obj.IsValid:
                    return obj.Class.Name.ToString();
                case ObjectBase:
                    return "(invalid object)";
                case Array array:
                    return $"{array.Length} item(s)";
                default:
                    return value.ToString() ?? value.GetType().Name;
            }
        }
    }
}
