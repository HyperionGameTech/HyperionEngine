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

        public InspectorComponentViewModelBase(Entity? target)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _isSelecting = 0;
        }
    }

    public class InspectorComponentViewModel<T> : InspectorComponentViewModelBase where T : IComponent, allows ref struct
    {
        public InspectorComponentViewModel(Entity? target)
            : base(target)
        {
        }
    }
}
