using System;
using System.Diagnostics;
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

        private int _refreshState;
        private int _applyingModelValue;

        private bool _isEditing;

        public Property Property => _property;

        public string Label => _label;

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

            InitializeLabel(property);
        }

        protected InspectorPropertyViewModelBase(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly = false)
        {
            _target = null;
            _componentClassAddress = classAddress;
            _componentTargetResolver = targetAddressResolver ?? throw new ArgumentNullException(nameof(targetAddressResolver));
            _property = property;
            _isReadOnly = isReadOnly;

            InitializeLabel(property);
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

        private void InitializeLabel(Property property)
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
