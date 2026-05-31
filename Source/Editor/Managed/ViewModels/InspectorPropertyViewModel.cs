using System;
using System.Diagnostics;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public abstract class InspectorPropertyViewModelBase : ViewModelBase
    {
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
        private string _label;
        protected int _isRefreshing;

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

        // Called on the sim thread immediately after a property value write succeeds.
        public Action? PostWriteCallback { get; set; }

        protected InspectorPropertyViewModelBase(ObjectBase? target, Property property, bool isReadOnly = false)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _property = property;
            _isReadOnly = isReadOnly;
            _isRefreshing = 0;

            InitializeLabel(property);
        }

        protected InspectorPropertyViewModelBase(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly = false)
        {
            _target = null;
            _componentClassAddress = classAddress;
            _componentTargetResolver = targetAddressResolver ?? throw new ArgumentNullException(nameof(targetAddressResolver));
            _property = property;
            _isReadOnly = isReadOnly;
            _isRefreshing = 0;

            InitializeLabel(property);
        }

        /// <summary>Delegate-based constructor used when there is no backing Property (e.g. array elements).</summary>
        protected InspectorPropertyViewModelBase(string label, TypeInfo typeInfoHint, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly = false)
        {
            _target = null;
            _property = Property.Invalid;
            _isReadOnly = isReadOnly;
            _isRefreshing = 0;
            _valueGetter = getter ?? throw new ArgumentNullException(nameof(getter));
            _valueSetter = setter ?? throw new ArgumentNullException(nameof(setter));
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
        
        protected void CommitPropertyChange(string actionText, BoxedValue newValue)
        {
            // Capture old value for undo (we are already on the sim thread).
            object? oldValueObj = null;

            try
            {
                using BoxedValue old = GetPropertyValue();
                oldValueObj = old.GetValue();
            }
            catch
            {
                /* If we can't read the old value, undo will be a no-op */
            }

            object? newValueObj = newValue.GetValue();

            if (oldValueObj != null && oldValueObj.Equals(newValueObj))
            {
                return;
            }

            EditorProject? project = EngineManager.CurrentProject;
            Debug.Assert(project != null, "No active project found when committing property change");

            // Capture all members we need so we don't need to actually capture 'this'
            IntPtr capturedClassAddress = _componentClassAddress;
            Func<IntPtr>? capturedResolver = _componentTargetResolver;
            ObjectBase? capturedTarget = _target;
            Property capturedProperty = _property;
            Func<BoxedValue>? capturedGetter = _valueGetter;
            Action<BoxedValue>? capturedSetter = _valueSetter;
            Action? capturedPostWrite = PostWriteCallback;
            InspectorPropertyViewModelBase capturedThis = this;

            void ApplyValue(object? valueObj)
            {
                if (valueObj == null)
                {
                    return;
                }

                try
                {
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

                    capturedPostWrite?.Invoke();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Editor action failed for property '{capturedProperty.Name}': {ex.Message}");
                }

                Dispatcher.UIThread.Post(() => capturedThis.RefreshValue());
            }

            EditorAction action = new EditorAction(
                actionText,
                execute: (_, _) => ApplyValue(newValueObj),
                revert:  (_, _) => ApplyValue(oldValueObj)
            );

            project.ActionStack.PushAction(action);
        }

        public virtual void CommitValue() { }

        private ICommand? _commitValueCommand;
        public ICommand CommitValueCommand => _commitValueCommand ??= new RelayCommand(CommitValue);

        protected bool IsTargetValid =>
            _valueGetter != null || _componentTargetResolver != null || (_target?.IsValid ?? false);

        public abstract void RefreshValue();

        internal static bool IsNameType(TypeInfo typeInfo)
        {
            if (typeInfo.Class?.Name is Name typeName)
            {
                return typeName == "Name";
            }

            return false;
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
