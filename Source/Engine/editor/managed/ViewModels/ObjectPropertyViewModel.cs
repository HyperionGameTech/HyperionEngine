using System;
using System.Threading;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ObjectPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const int MaxDepth = 4;

        private readonly int _depth;

        private ComponentSubObjectViewModel? _subObject;
        public ComponentSubObjectViewModel? SubObject
        {
            get => _subObject;
            private set => SetProperty(ref _subObject, value);
        }

        private bool _hasSubObject;
        public bool HasSubObject
        {
            get => _hasSubObject;
            private set => SetProperty(ref _hasSubObject, value);
        }

        public ObjectPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;
        }

        public ObjectPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;
        }

        public override bool ShowInlineLabel => false;

        public override void RefreshValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = GetPropertyValue();
                    object? val = boxed.GetValue();

                    ComponentSubObjectViewModel? subObjectVm = null;

                    if (val is ObjectBase obj && obj.IsValid && _depth < MaxDepth)
                    {
                        subObjectVm = new ComponentSubObjectViewModel(_property.Name.ToString(), obj, _depth + 1);
                    }

                    string displayName = val is ObjectBase o && o.IsValid
                        ? o.Class.Name.ToString()
                        : "(None)";

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        Value = displayName;
                        SubObject = subObjectVm;
                        HasSubObject = subObjectVm != null;
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read object property '{_property.Name}': {ex.Message}");
                }
            });
        }
    }
}
