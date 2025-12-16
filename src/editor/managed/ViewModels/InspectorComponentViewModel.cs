using System;
using System.Windows.Input;
using System.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorComponentViewModelBase : ViewModelBase
    {
        private readonly Entity _target;
        private int _isSelecting;

        public string Label { get; }

        public InspectorComponentViewModelBase(Entity? target, string label)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _isSelecting = 0;
            Label = label;
        }
    }

    public class InspectorComponentViewModel<T> : InspectorComponentViewModelBase where T : IComponent, allows ref struct
    {
        public InspectorComponentViewModel(Entity? target)
            : base(target, GetLabel())
        {
        }

        private static string GetLabel()
        {
            // Prefer reflected class name when available; fall back to type name
            Class? cls = Class.GetClass(typeof(T));

            if (cls != null)
            {
                return cls.Value.Name.ToString();
            }

            string typeName = typeof(T).Name;

            return typeName.EndsWith("Component", StringComparison.Ordinal)
                ? typeName
                : typeName + " Component";
        }
    }
}
