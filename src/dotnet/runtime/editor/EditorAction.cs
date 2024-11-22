using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="IEditorAction")]
    public abstract class IEditorAction : HypObject
    {
        public IEditorAction()
        {
        }

        public abstract Name GetName();
        public abstract void Execute();
        public abstract void Revert();
    }
}