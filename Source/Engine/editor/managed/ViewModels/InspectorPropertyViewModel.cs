using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public abstract class InspectorPropertyViewModelBase : ViewModelBase
    {
        protected readonly ObjectBase? _target;
        protected readonly Property _property;
        protected readonly bool _isReadOnly;

        private readonly IntPtr _componentClassAddress;
        private readonly Func<IntPtr>? _componentTargetResolver;

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

        public bool ShowTextValue => !IsTextEditable && !IsEnumEditable && !IsEnumFlagsEditable && !IsNumericEditable;

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
            if (_componentTargetResolver != null)
            {
                return _property.Get(_componentClassAddress, _componentTargetResolver());
            }

            return _property.Get(_target!);
        }

        protected void SetPropertyValue(BoxedValue value)
        {
            if (_componentTargetResolver != null)
            {
                _property.Set(_componentClassAddress, _componentTargetResolver(), value);
            }
            else
            {
                _property.Set(_target!, value);
            }
        }

        protected bool IsTargetValid =>
            _componentTargetResolver != null || (_target?.IsValid ?? false);

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
