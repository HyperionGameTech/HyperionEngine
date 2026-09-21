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
    /// <summary>One object a property row reads from and writes to. A multi-selection row has one per selected object.</summary>
    public sealed class PropertyTarget
    {
        public PropertyTarget(Func<BoxedValue> get, Action<BoxedValue> set, Action? preWrite = null, Action? postWrite = null, ObjectBase? owner = null)
        {
            Get = get;
            Set = set;
            PreWrite = preWrite;
            PostWrite = postWrite;
            Owner = owner;
        }

        // The object the property is read directly from, when there is one (entity-level rows) - used
        // to route edits into that entity's swatch overrides.
        public ObjectBase? Owner { get; }

        public Func<BoxedValue> Get { get; }

        public Action<BoxedValue> Set { get; }

        // Sim thread. Re-reads any cached copy of the containing value before it is read for a write.
        public Action? PreWrite { get; }

        // Sim thread. Runs after each write, e.g. to mark the owning entity dirty.
        public Action? PostWrite { get; }

        public static PropertyTarget ForObject(ObjectBase target, Property property, Action? postWrite = null)
            => new PropertyTarget(() => property.Get(target), value => property.Set(target, value), null, postWrite, target);

        public static PropertyTarget ForAddress(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, Action? preWrite = null, Action? postWrite = null)
            => new PropertyTarget(
                () => property.Get(classAddress, targetAddressResolver()),
                value => property.Set(classAddress, targetAddressResolver(), value),
                preWrite,
                postWrite);
    }

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

        // The other selected objects this row also edits. Written once on the UI thread before the
        // first refresh, read on the sim thread.
        private IReadOnlyList<PropertyTarget> _peers = Array.Empty<PropertyTarget>();

        private bool _hasMixedValues;
        /// <summary>True when the selected objects don't all hold the same value; the editor shows it blank. UI thread.</summary>
        public bool HasMixedValues
        {
            get => _hasMixedValues;
            protected set => SetProperty(ref _hasMixedValues, value);
        }

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

        protected IReadOnlyList<PropertyTarget> Peers => Volatile.Read(ref _peers);

        /// <summary>True when this row edits several selected objects at once.</summary>
        public bool IsMultiTarget => Peers.Count > 0;

        /// <summary>False for editors that can't yet apply one edit across several objects; multi-selection leaves those rows out.</summary>
        public virtual bool SupportsMultipleTargets => true;

        /// <summary>
        /// Makes this row also read from and write to the given objects, alongside its own target.
        /// UI thread, before the row is shown. Returns false (attaching nothing) when the editor
        /// can't edit several objects at once.
        /// </summary>
        internal bool AttachPeers(IReadOnlyList<PropertyTarget> peers)
        {
            if (peers.Count == 0)
            {
                return true;
            }

            if (!SupportsMultipleTargets)
            {
                return false;
            }

            Volatile.Write(ref _peers, peers);

            return OnPeersAttached();
        }

        // Containers pass the peers on to the rows they own. Returning false drops this row too.
        protected virtual bool OnPeersAttached() => true;

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
            WritePropertyValue(value);
            PostWriteCallback?.Invoke();
        }

        private void WritePropertyValue(BoxedValue value)
        {
            if (_valueSetter != null)
            {
                _valueSetter(value);
            }
            else if (_valueGetter != null)
            {
                throw new InvalidOperationException($"'{Label}' has no setter");
            }
            else if (_componentTargetResolver != null)
            {
                _property.Set(_componentClassAddress, _componentTargetResolver(), value);
            }
            else
            {
                _property.Set(_target!, value);
            }
        }

        // Built per commit so it picks up the current callbacks.
        private PropertyTarget CreateOwnTarget()
        {
            return new PropertyTarget(GetPropertyValue, WritePropertyValue, PreWriteCallback, PostWriteCallback, IsEntityLevelRow ? _target : null);
        }

        /// <summary>Reads the value of this row's own target, then of each peer. Sim thread.</summary>
        protected List<T> ReadAllTargets<T>(Func<BoxedValue, T> read)
        {
            IReadOnlyList<PropertyTarget> peers = Peers;
            List<T> results = new List<T>(1 + peers.Count);

            using (BoxedValue own = GetPropertyValue())
            {
                results.Add(read(own));
            }

            foreach (PropertyTarget peer in peers)
            {
                using BoxedValue value = peer.Get();
                results.Add(read(value));
            }

            return results;
        }

        /// <summary>Reads every target's value; false when they don't all agree. Sim thread.</summary>
        protected bool TryReadSharedValue(out object? value)
        {
            List<object?> values = ReadAllTargets(boxed => boxed.GetValue());

            value = values[0];

            for (int i = 1; i < values.Count; i++)
            {
                if (!ValuesEqual(values[i], values[0]))
                {
                    value = null;
                    return false;
                }
            }

            return true;
        }

        // Bitmask edits are computed as ulong; returning the property's own integral type lets
        // objects whose value didn't change compare equal and be left alone.
        protected static object ToIntegralTypeOf(ulong value, object? existing) => existing switch
        {
            uint => (uint)value,
            int => (int)value,
            ushort => (ushort)value,
            short => (short)value,
            byte => (byte)value,
            sbyte => (sbyte)value,
            long => (long)value,
            _ => value
        };

        // Arrays come back from BoxedValue as fresh object[]/byte[] instances, so compare them by content.
        internal static bool ValuesEqual(object? a, object? b)
        {
            if (a is object?[] arrayA && b is object?[] arrayB)
            {
                if (arrayA.Length != arrayB.Length)
                {
                    return false;
                }

                for (int i = 0; i < arrayA.Length; i++)
                {
                    if (!ValuesEqual(arrayA[i], arrayB[i]))
                    {
                        return false;
                    }
                }

                return true;
            }

            if (a is byte[] bytesA && b is byte[] bytesB)
            {
                return bytesA.AsSpan().SequenceEqual(bytesB);
            }

            return Equals(a, b);
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
            if (_valueSetter != null || _componentTargetResolver != null || _target is not Entity ownEntity)
            {
                return false;
            }

            // A multi-selection has no single edit-context entity; each entity is routed on its own.
            if (!IsMultiTarget
                && (SwatchOverrideEditContext.CurrentEntity is not Entity contextEntity
                    || contextEntity.NativeAddress != ownEntity.NativeAddress))
            {
                return false;
            }

            return TryWriteToSwatchOverride(ownEntity, value);
        }

        /// <summary>The same, for a multi-selection peer's copy of a container value.</summary>
        protected bool TryWritePeerContainerValueToSwatchOverride(PropertyTarget peer, BoxedValue value)
        {
            if (_valueSetter != null || _componentTargetResolver != null || peer.Owner is not Entity peerEntity)
            {
                return false;
            }

            return TryWriteToSwatchOverride(peerEntity, value);
        }

        private bool TryWriteToSwatchOverride(Entity overrideEntity, BoxedValue value)
        {
            if (_property.Name == new Name("Name", weak: true))
            {
                return false;
            }

            if (!overrideEntity.IsValid
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
        /// Writes a new value to every target through the project's action stack. Must be called on the sim thread.
        /// </summary>
        protected void CommitPropertyChange(string actionText, BoxedValue newValue)
        {
            object? newValueObj = newValue.GetValue();

            CommitChange(actionText, _ => newValueObj);
        }

        /// <summary>
        /// Writes a value computed from each target's current value, so an edit to part of a value
        /// (one vector component, one flag) keeps the rest of every selected object's own value.
        /// Must be called on the sim thread.
        /// </summary>
        protected void CommitPropertyChange(string actionText, Func<BoxedValue, object?> computeNewValue)
        {
            CommitChange(actionText, current => current != null
                ? computeNewValue(current)
                : throw new InvalidOperationException($"Failed to read '{Label}'"));
        }

        private void CommitChange(string actionText, Func<BoxedValue?, object?> computeNewValue)
        {
            if (_isReadOnly)
            {
                return;
            }

            List<PropertyTarget> targets = [CreateOwnTarget(), .. Peers];
            List<(PropertyTarget Target, object? OldValue, bool HasOldValue, object? NewValue)> changes = new();

            foreach (PropertyTarget target in targets)
            {
                // Bring any cached copy of the containing value up to date before reading the old value,
                // otherwise both the equality check below and the write itself use a stale snapshot.
                target.PreWrite?.Invoke();

                BoxedValue? current = null;
                object? oldValueObj = null;
                bool oldValueCaptured = false;

                try
                {
                    current = target.Get();
                    oldValueObj = current.GetValue();
                    oldValueCaptured = true;
                }
                catch
                {
                    /* If we can't read the old value, undo will be a no-op */
                }

                object? newValueObj;

                try
                {
                    newValueObj = computeNewValue(current);
                }
                finally
                {
                    current?.Dispose();
                }

                if (oldValueCaptured && ValuesEqual(oldValueObj, newValueObj))
                {
                    continue;
                }

                changes.Add((target, oldValueObj, oldValueCaptured, newValueObj));
            }

            if (changes.Count == 0)
            {
                // Nothing changed, but the UI may be showing an unparsed/optimistic value - put it back in sync.
                Dispatcher.UIThread.Post(RefreshValue);
                return;
            }

            EditorProject? project = EngineManager.CurrentProject;
            Debug.Assert(project != null, "No active project found when committing property change");

            ///Swatch override routing
            if (!IsMultiTarget
                && _valueSetter == null && _componentTargetResolver == null
                && _property.Name != new Name("Name", weak: true)
                && SwatchOverrideEditContext.CurrentEntity is Entity overrideEntity
                && overrideEntity.IsValid
                && _target != null
                && _target.NativeAddress == overrideEntity.NativeAddress
                && SwatchOverrideEditContext.ActiveSwatchName is string contextSwatch)
            {
                // Edits target the active swatch's override when override mode is enabled, or
                // when the property is ALREADY overridden by that swatch (editing the existing
                // override directly). Otherwise the edit writes the base value.
                if (SwatchOverrideEditContext.OverrideModeActive
                    || EntitySwatchOverrides.IsPropertyOverridden(overrideEntity, new Name(contextSwatch), _property.Name))
                {
                    CommitSwatchOverrideChange(overrideEntity, contextSwatch, actionText, changes[0].NewValue);
                    return;
                }
            }

            // The same routing for a multi-selection, decided per entity: each selected entity's edit
            // goes into its own override set where a single-selection edit to it would.
            List<(PropertyTarget Target, object? OldValue, bool HasOldValue, object? NewValue, SwatchOverrideEdit? SwatchEdit)> edits = new();

            Name? activeSwatch = IsMultiTarget
                && IsEntityLevelRow
                && _property.Name != new Name("Name", weak: true)
                && SwatchOverrideEditContext.ActiveSwatchName is string activeSwatchName
                    ? new Name(activeSwatchName)
                    : (Name?)null;

            foreach ((PropertyTarget target, object? oldValueObj, bool hasOldValue, object? newValueObj) in changes)
            {
                SwatchOverrideEdit? swatchEdit = null;

                if (activeSwatch.HasValue
                    && target.Owner is Entity ownerEntity
                    && ownerEntity.IsValid
                    && (SwatchOverrideEditContext.OverrideModeActive
                        || EntitySwatchOverrides.IsPropertyOverridden(ownerEntity, activeSwatch.Value, _property.Name)))
                {
                    swatchEdit = PrepareSwatchOverrideEdit(ownerEntity, activeSwatch.Value, newValueObj);

                    if (swatchEdit == null)
                    {
                        continue;
                    }
                }

                edits.Add((target, oldValueObj, hasOldValue, newValueObj, swatchEdit));
            }

            if (edits.Count == 0)
            {
                Dispatcher.UIThread.Post(RefreshValue);
                return;
            }

            InspectorPropertyViewModelBase capturedThis = this;

            void ApplyValues(bool revert)
            {
                foreach ((PropertyTarget target, object? oldValueObj, bool hasOldValue, object? newValueObj, SwatchOverrideEdit? swatchEdit) in edits)
                {
                    try
                    {
                        if (swatchEdit != null)
                        {
                            if (revert)
                            {
                                swatchEdit.Revert();
                            }
                            else
                            {
                                swatchEdit.Apply();
                            }

                            continue;
                        }

                        if (revert && !hasOldValue)
                        {
                            continue;
                        }

                        // Undo/redo runs long after the original edit, so the cached copy has to be
                        // re-read here too.
                        target.PreWrite?.Invoke();

                        using BoxedValue bv = new BoxedValue(revert ? oldValueObj : newValueObj);
                        target.Set(bv);

                        target.PostWrite?.Invoke();
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Editor action failed for property '{capturedThis.Label}': {ex.Message}");
                    }
                }

                Dispatcher.UIThread.Post(() =>
                {
                    capturedThis.RefreshValue();
                    capturedThis.ValueChangedCallback?.Invoke();
                });
            }

            EditorAction action = new EditorAction(
                actionText,
                execute: (_, _) => ApplyValues(revert: false),
                revert: (_, _) => ApplyValues(revert: true)
            );

            if (project != null)
            {
                project.ActionStack.PushAction(action);
            }
            else
            {
                // No project to record undo against - still apply the edit.
                ApplyValues(revert: false);
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
            SwatchOverrideEdit? edit = PrepareSwatchOverrideEdit(entity, new Name(swatchName), newValueObj);

            if (edit == null)
            {
                return;
            }

            EditorAction action = new EditorAction(
                edit.RemovesOverride ? $"Revert Override ({swatchName}): {Label}" : $"Override ({swatchName}): {Label}",
                execute: (_, _) => edit.Apply(),
                revert: (_, _) => edit.Revert());

            EditorProject? overrideProject = EngineManager.CurrentProject;

            if (overrideProject != null)
            {
                overrideProject.ActionStack.PushAction(action);
            }
            else
            {
                // No project to record undo against - still apply the edit.
                edit.Apply();
            }

            Dispatcher.UIThread.Post(() =>
            {
                RefreshValue();
                ValueChangedCallback?.Invoke();
            });
        }

        private sealed record SwatchOverrideEdit(bool RemovesOverride, Action Apply, Action Revert);

        /// <summary>
        /// Works out how writing newValueObj into the entity's override set for the swatch changes it
        /// (auto-creating and applying the set if needed). Null when it changes nothing. Sim thread.
        /// </summary>
        private SwatchOverrideEdit? PrepareSwatchOverrideEdit(Entity entity, Name swatch, object? newValueObj)
        {
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
                return null;
            }

            if (wasOverridden && hadPreviousOverride && Equals(previousOverrideObj, newValueObj) && !Equals(baseValueObj, newValueObj))
            {
                return null;
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

            return new SwatchOverrideEdit(
                removeOverride,
                Apply: () => ApplyOverrideState(!removeOverride, newValueObj, true),
                Revert: () =>
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
